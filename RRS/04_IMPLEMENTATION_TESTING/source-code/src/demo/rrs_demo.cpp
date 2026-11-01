/*
 * rrs_demo.cpp - console walkthrough of the Sprint 1 features using the real
 * auth, inventory and search modules (no TUI yet).
 * Build target: rrs_demo. Run: ./build/rrs_demo
 */
#include "auth.h"
#include "auth_internal.h"
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
        case ERR_FORBIDDEN:         return "forbidden (admin only)";
        case ERR_SESSION_EXPIRED:   return "rejected (not logged in)";
        case ERR_DUPLICATE:         return "rejected (already exists)";
        case ERR_NO_ROUTE:          return "no route";
        default:                    return "error";
    }
}

void heading(const char* text) { std::printf("\n== %s ==\n", text); }

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

void search(const char* from, const char* to) {
    char title[64];
    std::snprintf(title, sizeof title, "Search %s -> %s on %s", from, to, kDate);
    heading(title);
    rrs_route_list_t out;
    rrs_status_t st = search_routes(from, to, kDate, nullptr, &out);
    if (st != RRS_OK) {
        std::printf("  %s\n", status_name(st));
        return;
    }
    for (int i = 0; i < out.count; ++i) {
        const rrs_route_t& r = out.routes[i];
        char dep[16], arr[16];
        fmt_time(r.legs[0].depart_min, dep, sizeof dep);
        fmt_time(r.legs[r.leg_count - 1].arrive_min, arr, sizeof arr);
        std::printf("  %d. %-9s -> %-9s %2dh%02dm  %5d km  ", i + 1, dep, arr,
                    r.total_minutes / 60, r.total_minutes % 60, r.total_distance_km);
        if (r.leg_count == 1) {
            std::printf("direct, train %s\n", r.legs[0].train_id);
        } else {
            std::printf("%d trains:", r.leg_count);
            for (int l = 0; l < r.leg_count; ++l)
                std::printf(" %s %s->%s", r.legs[l].train_id, r.legs[l].from, r.legs[l].to);
            std::printf("\n");
        }
    }
}

}  // namespace

int main() {
    std::printf("Railway Reservation System - Sprint 1 console demo\n");

    heading("Register passengers");
    step("riya@example.com", auth_register("riya@example.com", "Passw0rdOK", "Riya"));
    step("riya@example.com again", auth_register("RIYA@example.com", "Another1Pass", "Riya 2"));
    step("bad email 'riya.example.com'", auth_register("riya.example.com", "Passw0rdOK", "Riya"));
    step("weak password 'abc'", auth_register("arjun@example.com", "abc", "Arjun"));

    /* Admins cannot self-register; seed one for the demo. */
    auth_create_user_for_tests("admin@rrs.in", "Adm1nPassw0rd", "Station Admin", RRS_ROLE_ADMIN);

    heading("Log in");
    rrs_session_t passenger, admin, bad;
    step("riya, wrong password", auth_login("riya@example.com", "wrong1Pass", &bad));
    rrs_status_t st = auth_login("riya@example.com", "Passw0rdOK", &passenger);
    step("riya", st);
    std::printf("  %-40s %s\n", "  role", passenger.role == RRS_ROLE_ADMIN ? "ADMIN" : "PASSENGER");
    st = auth_login("admin@rrs.in", "Adm1nPassw0rd", &admin);
    step("admin@rrs.in (seeded admin)", st);
    std::printf("  %-40s %s\n", "  role", admin.role == RRS_ROLE_ADMIN ? "ADMIN" : "PASSENGER");

    heading("Admin adds trains");
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

    search("SBC", "NDLS");
    search("SBC", "LKO");

    heading("Admin retires 12627");
    step("retire 12627", train_retire(&admin, "12627"));
    search("SBC", "NDLS");

    heading("Log out");
    step("riya logs out", auth_logout(&passenger));
    step("riya tries to view a train afterwards", train_get(&passenger, "12627", &t4));
    return 0;
}
