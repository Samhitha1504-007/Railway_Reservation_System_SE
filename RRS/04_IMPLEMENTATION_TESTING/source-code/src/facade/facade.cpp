/* Facade between the TUI and the modules (C++ so it can reach the auth test hook for the demo seed). */
#include "facade.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "audit.h"
#include "auth.h"
#include "auth_internal.h"

rrs_status_t facade_init(const char* audit_path)
{
    if (audit_path && audit_path[0] != '\0') {
        const char* slash = strrchr(audit_path, '/');
        if (slash && slash != audit_path) {
            char dir[128];
            size_t n = (size_t)(slash - audit_path);
            if (n < sizeof dir) {
                memcpy(dir, audit_path, n);
                dir[n] = '\0';
                (void)mkdir(dir, 0755); /* an existing folder is fine */
            }
        }
    }
    (void)audit_init(audit_path);
    booking_init();
    return RRS_OK;
}

static rrs_halt_t halt(const char* station, int arrive, int depart, int km)
{
    rrs_halt_t h;
    memset(&h, 0, sizeof h);
    snprintf(h.station, sizeof h.station, "%s", station);
    h.arrive_min = arrive;
    h.depart_min = depart;
    h.distance_km = km;
    return h;
}

static void add_train(const rrs_session_t* admin, const char* id, const rrs_halt_t* halts,
                      int n, int sl, int a3, int a2)
{
    rrs_train_t t;
    memset(&t, 0, sizeof t);
    snprintf(t.train_id, sizeof t.train_id, "%s", id);
    memcpy(t.halts, halts, (size_t)n * sizeof(rrs_halt_t));
    t.halt_count = n;
    t.capacity[RRS_CLASS_SLEEPER] = sl;
    t.capacity[RRS_CLASS_AC3] = a3;
    t.capacity[RRS_CLASS_AC2] = a2;
    (void)train_create(admin, &t);
}

rrs_status_t facade_seed_demo(void)
{
    rrs_session_t admin;
    rrs_status_t st;
    rrs_halt_t a[3], b[2], c[2];

    /* Admins cannot self-register; the demo seeds one. */
    (void)auth_create_user_for_tests("admin@rrs.in", "Adm1nPassw0rd", "Station Admin",
                                     RRS_ROLE_ADMIN);
    st = auth_login("admin@rrs.in", "Adm1nPassw0rd", &admin);
    if (st != RRS_OK) {
        return st;
    }
    a[0] = halt("SBC", 360, 360, 0);
    a[1] = halt("MAS", 660, 670, 360);
    a[2] = halt("NDLS", 1800, 1800, 2400);
    b[0] = halt("SBC", 1200, 1200, 0);
    b[1] = halt("NDLS", 3210, 3210, 2365);
    c[0] = halt("NDLS", 420, 420, 0);
    c[1] = halt("LKO", 810, 810, 512);
    add_train(&admin, "12627", a, 3, 400, 200, 100);
    add_train(&admin, "22691", b, 2, 0, 300, 150);
    add_train(&admin, "12004", c, 2, 0, 400, 200);
    (void)auth_logout(&admin);
    return RRS_OK;
}

rrs_status_t facade_search(const rrs_session_t* session, const char* from, const char* to,
                           const char* date, rrs_class_t cls, rrs_concession_t concession,
                           rrs_sort_key_t sort, const rrs_filter_t* filter,
                           rrs_option_list_t* out)
{
    rrs_route_list_t found;
    rrs_status_t st;

    if (out) {
        memset(out, 0, sizeof *out);
    }
    if (!session) {
        return ERR_SESSION_EXPIRED;
    }
    if (!from || !to || !date || !out) {
        return ERR_INVALID_INPUT;
    }
    st = auth_authorize(session, session->role);
    if (st != RRS_OK) {
        return st;
    }
    st = search_routes(from, to, date, NULL, &found);
    if (st != RRS_OK) {
        return st;
    }
    return search_rank(&found, cls, concession, sort, filter, out);
}

static void cancel_all(const rrs_session_t* session, const rrs_booking_t* b, int n)
{
    int i;
    for (i = 0; i < n; ++i) {
        (void)booking_cancel(session, b[i].pnr);
    }
}

rrs_status_t facade_book(const rrs_session_t* session, const rrs_route_t* route,
                         rrs_class_t cls, rrs_concession_t concession, const char* key,
                         rrs_booking_t* bookings, int* count)
{
    int i;
    rrs_status_t st;

    if (count) {
        *count = 0;
    }
    if (!session) {
        return ERR_SESSION_EXPIRED;
    }
    if (!route || !bookings || !count || !key || key[0] == '\0' || strlen(key) > 50 ||
        route->leg_count < 1 || route->leg_count > RRS_MAX_LEGS) {
        return ERR_INVALID_INPUT;
    }
    for (i = 0; i < route->leg_count; ++i) {
        rrs_route_t one;
        double fare = 0.0;
        char leg_key[RRS_IDEMPOTENCY_KEY_LEN];

        memset(&one, 0, sizeof one);
        one.leg_count = 1;
        one.legs[0] = route->legs[i];
        one.total_distance_km = route->legs[i].distance_km;
        st = fare_calculate(&one, cls, concession, &fare);
        if (st == RRS_OK) {
            snprintf(leg_key, sizeof leg_key, "%s-%d", key, i);
            st = booking_create(session, route->legs[i].train_id, cls, fare, leg_key,
                                &bookings[*count]);
        }
        if (st != RRS_OK) {
            cancel_all(session, bookings, *count);
            *count = 0;
            return st;
        }
        ++*count;
    }
    return RRS_OK;
}

rrs_status_t facade_my_bookings(const rrs_session_t* session, rrs_booking_t* out, int max,
                                int* count)
{
    return booking_list_by_passenger(session, out, max, count);
}

rrs_status_t facade_cancel(const rrs_session_t* session, const char* pnr)
{
    return booking_cancel(session, pnr);
}
