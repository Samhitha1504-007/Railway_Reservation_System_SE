/*
 * rrs_demo.cpp - console walkthrough of the features using the real auth,
 * inventory and search modules (no TUI yet).
 * Build target: rrs_demo. Run: ./build/rrs_demo
 */
#include "auth.h"
#include "auth_internal.h"
#include "booking.h"
#include "fare_eco.h"
#include "inventory.h"
#include "search.h"

#include <cstdio>
#include <cstring>

namespace {

const char* kDate = "2026-11-02";

const char* status_name(rrs_status_t st) {
    switch (st) {
        case RRS_OK:                return "OK";
        case ERR_INVALID_INPUT:     return "rejected (invalid input)";
        case ERR_AUTH_FAILED:       return "rejected (wrong email or password)";
        case ERR_FORBIDDEN:         return "forbidden (not allowed)";
        case ERR_SESSION_EXPIRED:   return "rejected (not logged in)";
        case ERR_DUPLICATE:         return "rejected (already exists)";
        case ERR_NO_ROUTE:          return "no route";
        case ERR_PAYMENT_FAILED:    return "rejected (payment failed)";
        case ERR_NOT_FOUND:         return "not found";
        default:                    return "error";
    }
}

/* Wait for Enter so each step can be explained while the demo runs. */
void pause() {
    std::printf("\n  [press Enter to continue]");
    std::fflush(stdout);
    int c;
    while ((c = std::getchar()) != '\n' && c != EOF) {}
}

void heading(const char* text, const char* explain) {
    pause();
    std::printf("\n== %s ==\n   %s\n\n", text, explain);
}

void step(const char* what, rrs_status_t st) {
    std::printf("  %-40s %s\n", what, status_name(st));
}

/* "HH:MM", with "+Nd" when the time falls on a later day */
void fmt_time(int minutes, char* buf, size_t cap) {
    int day = minutes / (24 * 60), m = minutes % (24 * 60);
    if (day > 0)
        std::snprintf(buf, cap, "%02d:%02d+%dd", m / 60, m % 60, day);
    else
        std::snprintf(buf, cap, "%02d:%02d", m / 60, m % 60);
}

rrs_halt_t halt(const char* station, int arrive, int depart, int km) {
    rrs_halt_t h;
    std::memset(&h, 0, sizeof h);
    std::snprintf(h.station, sizeof h.station, "%s", station);
    h.arrive_min = arrive;
    h.depart_min = depart;
    h.distance_km = km;
    return h;
}

rrs_train_t train(const char* id, std::initializer_list<rrs_halt_t> halts, int sleeper, int ac3, int ac2) {
    rrs_train_t t;
    std::memset(&t, 0, sizeof t);
    std::snprintf(t.train_id, sizeof t.train_id, "%s", id);
    for (const rrs_halt_t& h : halts) t.halts[t.halt_count++] = h;
    t.capacity[RRS_CLASS_SLEEPER] = sleeper;
    t.capacity[RRS_CLASS_AC3] = ac3;
    t.capacity[RRS_CLASS_AC2] = ac2;
    return t;
}

int hm(int h, int m) { return h * 60 + m; }

const char* sort_name(rrs_sort_key_t key) {
    return key == RRS_SORT_FARE ? "cheapest first" : key == RRS_SORT_ECO ? "greenest first" : "fastest first";
}

/* Search, then price, score and sort the options (AC 3-Tier). */
void search(const char* from, const char* to, const char* explain,
            rrs_sort_key_t key = RRS_SORT_TIME, rrs_concession_t concession = RRS_CONCESSION_NONE) {
    char title[96];
    std::snprintf(title, sizeof title, "Search %s -> %s on %s, AC 3-Tier, %s%s", from, to, kDate,
                  sort_name(key), concession == RRS_CONCESSION_SENIOR ? ", senior citizen" : "");
    heading(title, explain);
    rrs_route_list_t found;
    rrs_status_t st = search_routes(from, to, kDate, nullptr, &found);
    if (st != RRS_OK) {
        std::printf("  %s\n", status_name(st));
        return;
    }
    rrs_option_list_t out;
    search_rank(&found, RRS_CLASS_AC3, concession, key, nullptr, &out);
    for (int i = 0; i < out.count; ++i) {
        const rrs_route_t& r = out.options[i].route;
        char dep[16], arr[16];
        fmt_time(r.legs[0].depart_min, dep, sizeof dep);
        fmt_time(r.legs[r.leg_count - 1].arrive_min, arr, sizeof arr);
        std::printf("  %d. %-5s -> %-8s %2dh%02dm %5d km   Rs %5.0f   %5.1f kg CO2 (eco %d)   ", i + 1,
                    dep, arr, r.total_minutes / 60, r.total_minutes % 60, r.total_distance_km,
                    out.options[i].fare, out.options[i].eco.co2_kg, out.options[i].eco.score);
        if (r.leg_count == 1) {
            std::printf("direct %s\n", r.legs[0].train_id);
        } else {
            for (int l = 0; l < r.leg_count; ++l)
                std::printf("%s%s %s->%s", l ? ", " : "", r.legs[l].train_id, r.legs[l].from, r.legs[l].to);
            std::printf("\n");
        }
    }
}

}  // namespace

int main() {
    std::printf("Railway Reservation System - console demo\n");

    heading("Register passengers", "Riya signs up; then we try a duplicate email, a bad email and a weak password.");
    step("riya@example.com", auth_register("riya@example.com", "Passw0rdOK", "Riya"));
    step("riya@example.com again", auth_register("RIYA@example.com", "Another1Pass", "Riya 2"));
    step("bad email 'riya.example.com'", auth_register("riya.example.com", "Passw0rdOK", "Riya"));
    step("weak password 'abc'", auth_register("arjun@example.com", "abc", "Arjun"));

    /* Admins cannot self-register; seed one for the demo. */
    auth_create_user_for_tests("admin@rrs.in", "Adm1nPassw0rd", "Station Admin", RRS_ROLE_ADMIN);

    heading("Log in", "A wrong password first, then Riya and the admin log in and get a role.");
    rrs_session_t passenger, admin, bad;
    step("riya, wrong password", auth_login("riya@example.com", "wrong1Pass", &bad));
    rrs_status_t st = auth_login("riya@example.com", "Passw0rdOK", &passenger);
    step("riya", st);
    std::printf("  %-40s %s\n", "  role", passenger.role == RRS_ROLE_ADMIN ? "ADMIN" : "PASSENGER");
    st = auth_login("admin@rrs.in", "Adm1nPassw0rd", &admin);
    step("admin@rrs.in (seeded admin)", st);
    std::printf("  %-40s %s\n", "  role", admin.role == RRS_ROLE_ADMIN ? "ADMIN" : "PASSENGER");

    heading("Admin adds trains", "The admin creates three trains; a passenger tries too, and a bad route is entered.");
    rrs_train_t t1 = train("12627", {halt("SBC", hm(6, 0), hm(6, 0), 0),
                                     halt("MAS", hm(11, 0), hm(11, 10), 360),
                                     halt("NDLS", hm(30, 0), hm(30, 0), 2400)}, 400, 200, 100);
    rrs_train_t t2 = train("22691", {halt("SBC", hm(20, 0), hm(20, 0), 0),
                                     halt("NDLS", hm(53, 30), hm(53, 30), 2365)}, 0, 300, 150);
    rrs_train_t t3 = train("12004", {halt("NDLS", hm(7, 0), hm(7, 0), 0),
                                     halt("LKO", hm(13, 30), hm(13, 30), 512)}, 0, 400, 200);
    step("12627  SBC -> MAS -> NDLS", train_create(&admin, &t1));
    step("22691  SBC -> NDLS", train_create(&admin, &t2));
    step("12004  NDLS -> LKO", train_create(&admin, &t3));
    rrs_train_t t4 = train("99999", {halt("SBC", 0, 0, 0), halt("MAS", 300, 300, 360)}, 10, 0, 0);
    step("passenger tries to add 99999", train_create(&passenger, &t4));
    rrs_train_t loop = train("11111", {halt("SBC", 0, 0, 0), halt("SBC", 300, 300, 100)}, 10, 0, 0);
    step("admin adds a route that repeats SBC", train_create(&admin, &loop));

    search("SBC", "NDLS", "Bengaluru to New Delhi: two direct trains, fastest first.");
    search("SBC", "LKO", "Bengaluru to Lucknow: no direct train, so the search changes at New Delhi.");
    search("SBC", "NDLS", "Same trip sorted by fare: the fare depends on distance and class.",
           RRS_SORT_FARE);
    search("SBC", "NDLS", "A senior citizen gets 40% off the distance charge.",
           RRS_SORT_FARE, RRS_CONCESSION_SENIOR);
    search("SBC", "NDLS", "Greenest first: lowest CO2 per passenger, compared with driving.",
           RRS_SORT_ECO);

    heading("Book tickets", "Riya books AC 3-Tier on 22691; a retry with the same key returns the same PNR; a failed payment books nothing.");
    booking_init();
    rrs_booking_t b1, b1_retry, bad_pay;
    step("riya books 22691 AC3 (key k-1)", booking_create(&passenger, "22691", RRS_CLASS_AC3, 2100.0, "k-1", &b1));
    std::printf("  %-40s %s (%s)\n", "  PNR / status", b1.pnr, b1.status == RRS_BOOKING_CONFIRMED ? "CONFIRMED" : "WAITLISTED");
    step("retry with the same key k-1", booking_create(&passenger, "22691", RRS_CLASS_AC3, 2100.0, "k-1", &b1_retry));
    std::printf("  %-40s %s\n", "  same PNR returned", std::strcmp(b1.pnr, b1_retry.pnr) == 0 ? "yes, no duplicate" : "NO");
    booking_set_mock_payment_result(0);
    step("booking with a failed payment", booking_create(&passenger, "22691", RRS_CLASS_AC3, 2100.0, "k-2", &bad_pay));
    booking_set_mock_payment_result(-1);

    heading("Waitlist and auto-promotion", "Train 12951 has one AC3 seat. Riya takes it, Arjun is waitlisted; when Riya cancels, Arjun is promoted.");
    rrs_train_t t5 = train("12951", {halt("SBC", hm(8, 0), hm(8, 0), 0), halt("MAS", hm(14, 0), hm(14, 0), 360)}, 0, 1, 0);
    step("admin adds 12951 (1 AC3 seat)", train_create(&admin, &t5));
    auth_register("arjun@example.com", "Passw0rdOK", "Arjun");
    rrs_session_t arjun;
    auth_login("arjun@example.com", "Passw0rdOK", &arjun);
    rrs_booking_t rb, ab, ab2;
    step("riya books the only seat", booking_create(&passenger, "12951", RRS_CLASS_AC3, 900.0, "r-1", &rb));
    step("arjun books the same train", booking_create(&arjun, "12951", RRS_CLASS_AC3, 900.0, "a-1", &ab));
    std::printf("  %-40s %s, position %d\n", "  arjun's status",
                ab.status == RRS_BOOKING_WAITLISTED ? "WAITLISTED" : "CONFIRMED", ab.waitlist_position);
    step("riya cancels her ticket", booking_cancel(&passenger, rb.pnr));
    booking_get(&arjun, ab.pnr, &ab2);
    std::printf("  %-40s %s\n", "  arjun's status now",
                ab2.status == RRS_BOOKING_CONFIRMED ? "CONFIRMED (auto-promoted)" : "still waitlisted");
    step("arjun tries to cancel riya's old PNR", booking_cancel(&arjun, rb.pnr));

    heading("Admin retires 12627", "The admin takes train 12627 out of service.");
    step("retire 12627", train_retire(&admin, "12627"));
    search("SBC", "NDLS", "Same search again: 12627 is gone.");

    heading("Log out", "Riya logs out; her old session should stop working.");
    step("riya logs out", auth_logout(&passenger));
    step("riya tries to view a train afterwards", train_get(&passenger, "12627", &t4));
    std::printf("\nEnd of demo.\n");
    return 0;
}
