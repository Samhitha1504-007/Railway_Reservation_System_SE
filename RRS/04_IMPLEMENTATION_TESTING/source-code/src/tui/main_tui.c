/*
 * ncurses front end for the Railway Reservation System.
 *
 * Passenger flow, search to ticket in four screens (NFR-004):
 *   Search form -> Results -> Confirm and pay -> Ticket
 * Other screens: Login/Register, Main menu, My bookings (cancel), Admin trains.
 * Every action goes through the facade; fares are never typed in by the user.
 */
#include "tui.h"

#include <ctype.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "auth.h"
#include "facade.h"

#define LINE_MAX_INPUT 64

typedef struct app_s {
    rrs_session_t session;
    int           logged_in;
    char          message[96];   /* one-line status shown at the bottom */
} app_t;

static const struct app_s* g_app; /* for the status line drawn by screen() */

static const char* class_name(rrs_class_t c)
{
    return c == RRS_CLASS_SLEEPER ? "Sleeper" : c == RRS_CLASS_AC3 ? "AC 3-Tier" : "AC 2-Tier";
}

static const char* status_text(rrs_status_t st)
{
    switch (st) {
    case RRS_OK:                return "OK";
    case ERR_INVALID_INPUT:     return "Invalid input, please check the fields";
    case ERR_AUTH_FAILED:       return "Wrong email or password";
    case ERR_FORBIDDEN:         return "You are not allowed to do that";
    case ERR_SESSION_EXPIRED:   return "Session expired, please log in again";
    case ERR_NOT_FOUND:         return "Not found";
    case ERR_DUPLICATE:         return "Already exists";
    case ERR_CAPACITY_CONFLICT: return "No capacity available";
    case ERR_PAYMENT_FAILED:    return "Payment failed, nothing was booked";
    case ERR_NO_ROUTE:          return "No route between those stations";
    default:                    return "Something went wrong";
    }
}

static void set_msg(app_t* app, const char* text)
{
    snprintf(app->message, sizeof app->message, "%s", text);
}

/* ---- drawing helpers ---- */

static void footer(const app_t* app);

static void screen(const char* title, const char* hint)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)cols;
    erase();
    attron(A_BOLD);
    mvprintw(0, 2, "Railway Reservation System  |  %s", title);
    attroff(A_BOLD);
    mvhline(1, 0, ACS_HLINE, COLS);
    mvprintw(rows - 2, 2, "%s", hint);
    if (g_app) {
        footer(g_app);
    }
}

static void footer(const app_t* app)
{
    int rows = LINES;
    if (app->message[0] != '\0') {
        attron(A_REVERSE);
        mvprintw(rows - 1, 0, " %-*s", COLS - 1, app->message);
        attroff(A_REVERSE);
    }
    refresh();
}

/* Prompt for a line of text at (row, col). `secret` hides what is typed.
 * Returns 0 on Enter, -1 if the user pressed Esc. */
static int read_field(int row, int col, const char* label, char* buf, size_t cap, int secret)
{
    size_t len = 0;
    int ch;

    buf[0] = '\0';
    mvprintw(row, col, "%-18s ", label);
    clrtoeol();
    refresh();
    noecho();
    curs_set(1);
    while ((ch = getch()) != '\n' && ch != '\r' && ch != KEY_ENTER) {
        if (ch == 27) {
            curs_set(0);
            return -1;
        }
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b') && len > 0) {
            buf[--len] = '\0';
            addstr("\b \b");
        } else if (ch >= 32 && ch < 127 && len + 1 < cap && len < LINE_MAX_INPUT) {
            buf[len++] = (char)ch;
            buf[len] = '\0';
            addch(secret ? '*' : (chtype)ch);
        }
        refresh();
    }
    curs_set(0);
    return 0;
}

/* Show `n` menu lines and return the chosen index (0-based), or -1 on Esc/q. */
static int choose(int top, const char* const* items, int n)
{
    int sel = 0, ch, i;
    for (;;) {
        for (i = 0; i < n; ++i) {
            if (i == sel) {
                attron(A_REVERSE);
            }
            mvprintw(top + i, 4, " %-60s", items[i]);
            if (i == sel) {
                attroff(A_REVERSE);
            }
        }
        refresh();
        ch = getch();
        if (ch == KEY_UP || ch == 'k') {
            sel = (sel + n - 1) % n;
        } else if (ch == KEY_DOWN || ch == 'j') {
            sel = (sel + 1) % n;
        } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            return sel;
        } else if (ch == 27 || ch == 'q') {
            return -1;
        } else if (ch >= '1' && ch < '1' + n) {
            return ch - '1';
        }
    }
}

static void wait_key(void)
{
    mvprintw(LINES - 3, 2, "Press any key to continue");
    refresh();
    (void)getch();
}

static void today(char* out, size_t cap)
{
    time_t now = time(NULL);
    struct tm tmv;
    struct tm* tp = localtime(&now);
    if (tp) {
        tmv = *tp;
        strftime(out, cap, "%Y-%m-%d", &tmv);
    } else {
        snprintf(out, cap, "2026-11-02");
    }
}

static void upcase(char* s)
{
    for (; *s; ++s) {
        *s = (char)toupper((unsigned char)*s);
    }
}

static void fmt_time(int minutes, char* buf, size_t cap)
{
    int day = minutes / 1440, m = minutes % 1440;
    if (day > 0) {
        snprintf(buf, cap, "%02d:%02d+%dd", m / 60, m % 60, day);
    } else {
        snprintf(buf, cap, "%02d:%02d", m / 60, m % 60);
    }
}

/* ---- screen 1: login / register ---- */

static void do_login(app_t* app)
{
    char email[LINE_MAX_INPUT + 1], pw[LINE_MAX_INPUT + 1];
    rrs_session_t s;
    rrs_status_t st;

    screen("Login", "Esc cancels");
    if (read_field(3, 4, "Email:", email, sizeof email, 0) < 0 ||
        read_field(5, 4, "Password:", pw, sizeof pw, 1) < 0) {
        return;
    }
    st = auth_login(email, pw, &s);
    memset(pw, 0, sizeof pw);
    if (st == RRS_OK) {
        app->session = s;
        app->logged_in = 1;
        set_msg(app, "Logged in");
    } else {
        set_msg(app, status_text(st));
    }
}

static void do_register(app_t* app)
{
    char email[LINE_MAX_INPUT + 1], pw[LINE_MAX_INPUT + 1], name[LINE_MAX_INPUT + 1];
    rrs_status_t st;

    screen("Register", "Password: 8-64 chars with upper, lower and a digit. Esc cancels");
    if (read_field(3, 4, "Name:", name, sizeof name, 0) < 0 ||
        read_field(5, 4, "Email:", email, sizeof email, 0) < 0 ||
        read_field(7, 4, "Password:", pw, sizeof pw, 1) < 0) {
        return;
    }
    st = auth_register(email, pw, name);
    memset(pw, 0, sizeof pw);
    set_msg(app, st == RRS_OK ? "Account created, you can log in now" : status_text(st));
}

static int welcome(app_t* app)
{
    static const char* const items[] = { "1. Login", "2. Register", "3. Quit" };
    int pick;

    screen("Welcome", "Up/Down or 1-3 to choose, Enter to select");
    mvprintw(3, 4, "Search trains, book tickets and manage them from one place.");
    pick = choose(5, items, 3);
    set_msg(app, "");
    if (pick == 0) {
        do_login(app);
    } else if (pick == 1) {
        do_register(app);
    } else {
        return 0;
    }
    return 1;
}

/* ---- screens 2-5: search, results, confirm, ticket ---- */

static void show_ticket(app_t* app, const rrs_booking_t* b, int n, const rrs_route_option_t* opt)
{
    int i, row = 4;

    screen("Ticket", "Keep your PNR to view or cancel this ticket");
    mvprintw(3, 4, "Booking done. Total fare Rs %.0f", opt->fare);
    for (i = 0; i < n; ++i) {
        mvprintw(row++, 4, "PNR %s  train %s  %s  %s", b[i].pnr, b[i].train_id,
                 class_name(b[i].travel_class),
                 b[i].status == RRS_BOOKING_CONFIRMED ? "CONFIRMED" : "WAITLISTED");
        if (b[i].status == RRS_BOOKING_WAITLISTED) {
            mvprintw(row++, 6, "waitlist position %d (promoted automatically if a seat frees up)",
                     b[i].waitlist_position);
        }
    }
    set_msg(app, "Booked");
    footer(app);
    wait_key();
}

static void confirm_and_pay(app_t* app, const rrs_route_option_t* opt, rrs_class_t cls,
                            rrs_concession_t conc)
{
    static unsigned long attempt = 0;
    char key[32], ans[8];
    rrs_booking_t b[RRS_MAX_LEGS];
    int n = 0, l;
    rrs_status_t st;

    screen("Confirm and pay", "Type y to pay, anything else to go back");
    mvprintw(3, 4, "Class: %s    Total fare: Rs %.0f    Eco-score: %d (%.1f kg CO2)",
             class_name(cls), opt->fare, opt->eco.score, opt->eco.co2_kg);
    for (l = 0; l < opt->route.leg_count; ++l) {
        char dep[16], arr[16];
        const rrs_leg_t* g = &opt->route.legs[l];
        fmt_time(g->depart_min, dep, sizeof dep);
        fmt_time(g->arrive_min, arr, sizeof arr);
        mvprintw(5 + l, 4, "%s  %s %s -> %s %s", g->train_id, g->from, dep, g->to, arr);
    }
    footer(app);
    if (read_field(5 + opt->route.leg_count + 1, 4, "Pay now (y/n)?", ans, sizeof ans, 0) < 0 ||
        (ans[0] != 'y' && ans[0] != 'Y')) {
        set_msg(app, "Booking cancelled, nothing charged");
        return;
    }
    snprintf(key, sizeof key, "tui-%ld-%lu", (long)time(NULL), ++attempt);
    st = facade_book(&app->session, &opt->route, cls, conc, key, b, &n);
    if (st != RRS_OK) {
        set_msg(app, status_text(st));
        return;
    }
    show_ticket(app, b, n, opt);
}

static void results(app_t* app, const rrs_option_list_t* list, rrs_class_t cls,
                    rrs_concession_t conc)
{
    char lines[RRS_MAX_ROUTES][110];
    const char* ptrs[RRS_MAX_ROUTES];
    int i, pick;

    screen("Results", "Enter selects a journey, Esc goes back");
    if (list->count == 0) {
        mvprintw(3, 4, "No journeys match your filters.");
        set_msg(app, "No results");
        footer(app);
        wait_key();
        return;
    }
    mvprintw(3, 4, "     Depart  Arrive      Time   Fare   Eco   Trains");
    for (i = 0; i < list->count; ++i) {
        const rrs_route_t* r = &list->options[i].route;
        char dep[16], arr[16], trains[48] = "";
        int l;
        fmt_time(r->legs[0].depart_min, dep, sizeof dep);
        fmt_time(r->legs[r->leg_count - 1].arrive_min, arr, sizeof arr);
        for (l = 0; l < r->leg_count; ++l) {
            size_t used = strlen(trains);
            snprintf(trains + used, sizeof trains - used, "%s%s", l ? "+" : "",
                     r->legs[l].train_id);
        }
        snprintf(lines[i], sizeof lines[i], "%2d.  %-8s %-9s %2dh%02dm  Rs%5.0f  %3d   %s",
                 i + 1, dep, arr, r->total_minutes / 60, r->total_minutes % 60,
                 list->options[i].fare, list->options[i].eco.score, trains);
        ptrs[i] = lines[i];
    }
    footer(app);
    pick = choose(4, ptrs, list->count);
    if (pick >= 0) {
        confirm_and_pay(app, &list->options[pick], cls, conc);
    }
}

static void search_screen(app_t* app)
{
    static const char* const classes[] = { "1. Sleeper", "2. AC 3-Tier", "3. AC 2-Tier" };
    static const char* const sorts[] = { "1. Fastest first", "2. Cheapest first",
                                         "3. Greenest first" };
    static const char* const concs[] = { "1. No concession", "2. Senior citizen (40% off)",
                                         "3. Student (25% off)", "4. Child (50% off)" };
    char from[LINE_MAX_INPUT + 1], to[LINE_MAX_INPUT + 1], date[LINE_MAX_INPUT + 1];
    char dflt[16];
    rrs_class_t cls;
    rrs_sort_key_t key;
    rrs_concession_t conc;
    rrs_option_list_t list;
    rrs_status_t st;
    int pick;

    today(dflt, sizeof dflt);
    screen("Search", "Station codes such as SBC, MAS, NDLS, LKO. Esc cancels");
    if (read_field(3, 4, "From station:", from, sizeof from, 0) < 0 ||
        read_field(5, 4, "To station:", to, sizeof to, 0) < 0) {
        return;
    }
    mvprintw(7, 4, "Date (YYYY-MM-DD, Enter for %s):", dflt);
    if (read_field(8, 4, "Date:", date, sizeof date, 0) < 0) {
        return;
    }
    if (date[0] == '\0') {
        snprintf(date, sizeof date, "%s", dflt);
    }
    upcase(from);
    upcase(to);

    mvprintw(10, 4, "Class:");
    pick = choose(11, classes, 3);
    if (pick < 0) {
        return;
    }
    cls = (rrs_class_t)pick;
    mvprintw(15, 4, "Sort by:");
    pick = choose(16, sorts, 3);
    if (pick < 0) {
        return;
    }
    key = (rrs_sort_key_t)pick;
    screen("Search", "Concession applies to the distance charge. Esc cancels");
    mvprintw(3, 4, "Concession:");
    pick = choose(4, concs, 4);
    if (pick < 0) {
        return;
    }
    conc = (rrs_concession_t)pick;

    st = facade_search(&app->session, from, to, date, cls, conc, key, NULL, &list);
    if (st != RRS_OK) {
        set_msg(app, status_text(st));
        return;
    }
    results(app, &list, cls, conc);
}

/* ---- my bookings / cancel ---- */

static void my_bookings(app_t* app)
{
    rrs_booking_t all[64];
    char lines[64][100];
    const char* ptrs[64];
    int n = 0, i, pick;
    rrs_status_t st;

    screen("My bookings", "Enter on a ticket to cancel it, Esc goes back");
    st = facade_my_bookings(&app->session, all, 64, &n);
    if (st != RRS_OK) {
        set_msg(app, status_text(st));
        return;
    }
    if (n == 0) {
        mvprintw(3, 4, "You have no bookings yet.");
        footer(app);
        wait_key();
        return;
    }
    for (i = 0; i < n; ++i) {
        const char* state = all[i].status == RRS_BOOKING_CONFIRMED ? "CONFIRMED"
                          : all[i].status == RRS_BOOKING_WAITLISTED ? "WAITLISTED" : "CANCELLED";
        snprintf(lines[i], sizeof lines[i], "%-13s %-10s %-10s Rs%6.0f  %s", all[i].pnr,
                 all[i].train_id, class_name(all[i].travel_class), all[i].fare_amount, state);
        ptrs[i] = lines[i];
    }
    footer(app);
    pick = choose(3, ptrs, n);
    if (pick >= 0) {
        char ans[8];
        mvprintw(LINES - 4, 4, "Cancel %s? (y/n)", all[pick].pnr);
        if (read_field(LINES - 4, 4, "Cancel (y/n)?", ans, sizeof ans, 0) == 0 &&
            (ans[0] == 'y' || ans[0] == 'Y')) {
            st = facade_cancel(&app->session, all[pick].pnr);
            set_msg(app, st == RRS_OK ? "Ticket cancelled" : status_text(st));
        }
    }
}

/* ---- admin: add or retire a train ---- */

static void admin_add_train(app_t* app)
{
    char id[LINE_MAX_INPUT + 1], a[LINE_MAX_INPUT + 1], b[LINE_MAX_INPUT + 1];
    char km[LINE_MAX_INPUT + 1], dep[LINE_MAX_INPUT + 1], dur[LINE_MAX_INPUT + 1];
    char cap[LINE_MAX_INPUT + 1];
    rrs_train_t t;
    rrs_status_t st;
    int dist, depart, minutes, capacity;

    screen("Admin: add train", "A two-stop train. Esc cancels");
    if (read_field(3, 4, "Train id:", id, sizeof id, 0) < 0 ||
        read_field(4, 4, "From station:", a, sizeof a, 0) < 0 ||
        read_field(5, 4, "To station:", b, sizeof b, 0) < 0 ||
        read_field(6, 4, "Distance (km):", km, sizeof km, 0) < 0 ||
        read_field(7, 4, "Depart (minutes):", dep, sizeof dep, 0) < 0 ||
        read_field(8, 4, "Journey (minutes):", dur, sizeof dur, 0) < 0 ||
        read_field(9, 4, "Seats per class:", cap, sizeof cap, 0) < 0) {
        return;
    }
    dist = atoi(km);
    depart = atoi(dep);
    minutes = atoi(dur);
    capacity = atoi(cap);
    upcase(a);
    upcase(b);
    memset(&t, 0, sizeof t);
    snprintf(t.train_id, sizeof t.train_id, "%s", id);
    t.halt_count = 2;
    snprintf(t.halts[0].station, sizeof t.halts[0].station, "%s", a);
    t.halts[0].arrive_min = t.halts[0].depart_min = depart;
    snprintf(t.halts[1].station, sizeof t.halts[1].station, "%s", b);
    t.halts[1].arrive_min = t.halts[1].depart_min = depart + minutes;
    t.halts[1].distance_km = dist;
    t.capacity[RRS_CLASS_SLEEPER] = t.capacity[RRS_CLASS_AC3] = t.capacity[RRS_CLASS_AC2] =
        capacity;
    st = train_create(&app->session, &t);
    set_msg(app, st == RRS_OK ? "Train added" : status_text(st));
}

static void admin_retire_train(app_t* app)
{
    char id[LINE_MAX_INPUT + 1];
    rrs_status_t st;

    screen("Admin: retire train", "Esc cancels");
    if (read_field(3, 4, "Train id:", id, sizeof id, 0) < 0) {
        return;
    }
    st = train_retire(&app->session, id);
    set_msg(app, st == RRS_OK ? "Train retired" : status_text(st));
}

/* ---- main menu ---- */

static void menu(app_t* app)
{
    static const char* const pass_items[] = { "1. Search and book", "2. My bookings", "3. Logout" };
    static const char* const admin_items[] = { "1. Add a train", "2. Retire a train", "3. Logout" };
    int is_admin = app->session.role == RRS_ROLE_ADMIN;
    int pick;

    screen(is_admin ? "Admin menu" : "Main menu", "Up/Down or 1-3 to choose, Enter to select");
    footer(app);
    pick = choose(3, is_admin ? admin_items : pass_items, 3);
    set_msg(app, "");
    if (pick == 2 || pick < 0) {
        (void)auth_logout(&app->session);
        app->logged_in = 0;
        set_msg(app, "Logged out");
    } else if (is_admin) {
        if (pick == 0) {
            admin_add_train(app);
        } else {
            admin_retire_train(app);
        }
    } else if (pick == 0) {
        search_screen(app);
    } else {
        my_bookings(app);
    }
}

int rrs_tui_start(void)
{
    app_t app;

    memset(&app, 0, sizeof app);
    g_app = &app;
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    set_escdelay(25);

    for (;;) {
        if (!app.logged_in) {
            if (!welcome(&app)) {
                break;
            }
        } else {
            menu(&app);
        }
        footer(&app);
    }
    endwin();
    return 0;
}
