/*
 * Security validation pass (#36): SEC-001 to SEC-006 run end to end through the
 * real auth, inventory, search, booking and audit modules, as described in the
 * Test Plan section 5. Results are recorded in
 * RRS/04_IMPLEMENTATION_TESTING/security-validation/security_validation_report.md
 */
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "audit.h"
#include "audit_internal.h"
#include "auth.h"
#include "auth_internal.h"
#include "booking.h"
#include "inventory.h"
#include "inventory_internal.h"
#include "search.h"
#include "session.h"

namespace {

rrs_train_t train(const char* id, int seats) {
    rrs_train_t t;
    std::memset(&t, 0, sizeof t);
    std::snprintf(t.train_id, sizeof t.train_id, "%s", id);
    t.halt_count = 2;
    std::snprintf(t.halts[0].station, RRS_STATION_CODE_LEN, "SBC");
    std::snprintf(t.halts[1].station, RRS_STATION_CODE_LEN, "MAS");
    t.halts[1].arrive_min = t.halts[1].depart_min = 300;
    t.halts[1].distance_km = 360;
    t.capacity[RRS_CLASS_SLEEPER] = seats;
    return t;
}

std::vector<std::string> audit_lines() {
    std::vector<std::string> out;
    for (size_t i = 0; i < audit_entry_count_for_tests(); ++i) {
        std::string line;
        audit_entry_for_tests(i, line);
        out.push_back(line);
    }
    return out;
}

int count_lines_with(const std::vector<std::string>& lines, const std::string& a, const std::string& b = "") {
    int n = 0;
    for (const std::string& l : lines)
        if (l.find(a) != std::string::npos && (b.empty() || l.find(b) != std::string::npos)) ++n;
    return n;
}

}  // namespace

class SecurityValidation : public ::testing::Test {
protected:
    rrs_session_t admin, alice, bob;
    void SetUp() override {
        auth_reset_for_tests();
        inventory_reset_for_tests();
        booking_clear();
        booking_init();
        audit_reset_for_tests();
        ASSERT_EQ(RRS_OK, auth_create_user_for_tests("admin@rrs.in", "Adm1nPassw0rd", "Admin",
                                                     RRS_ROLE_ADMIN));
        ASSERT_EQ(RRS_OK, auth_register("alice@rrs.in", "Passw0rdOK", "Alice"));
        ASSERT_EQ(RRS_OK, auth_register("bob@rrs.in", "Passw0rdOK", "Bob"));   /* same password */
        ASSERT_EQ(RRS_OK, auth_login("admin@rrs.in", "Adm1nPassw0rd", &admin));
        ASSERT_EQ(RRS_OK, auth_login("alice@rrs.in", "Passw0rdOK", &alice));
        ASSERT_EQ(RRS_OK, auth_login("bob@rrs.in", "Passw0rdOK", &bob));
        rrs_train_t t = train("12627", 2);
        ASSERT_EQ(RRS_OK, train_create(&admin, &t));
        audit_reset_for_tests();   /* each test starts with an empty log */
    }
    void TearDown() override {
        rrs::session_set_fixed_time_for_tests(0);
        booking_clear();
        inventory_reset_for_tests();
        audit_reset_for_tests();
    }
};

/* SEC-001: inspect stored users; salted hashes only, salts differ per user. */
TEST_F(SecurityValidation, Sec001PasswordsAreStoredAsSaltedHashesOnly) {
    std::string a, b;
    ASSERT_TRUE(auth_get_stored_hash_for_tests("alice@rrs.in", a));
    ASSERT_TRUE(auth_get_stored_hash_for_tests("bob@rrs.in", b));
    EXPECT_EQ(std::string::npos, a.find("Passw0rdOK"));
    EXPECT_EQ(0u, a.rfind("pbkdf2$", 0));
    EXPECT_NE(a, b);   /* same password, different salt */
}

/* SEC-002: a passenger session calling an admin-only function gets ERR_FORBIDDEN. */
TEST_F(SecurityValidation, Sec002PassengerCannotCallAdminFunctions) {
    rrs_train_t t = train("99999", 5);
    EXPECT_EQ(ERR_FORBIDDEN, train_create(&alice, &t));
    t = train("12627", 50);
    EXPECT_EQ(ERR_FORBIDDEN, train_update(&alice, &t));
    EXPECT_EQ(ERR_FORBIDDEN, train_retire(&alice, "12627"));
    EXPECT_EQ(1, train_is_bookable("12627"));          /* nothing changed */
    EXPECT_EQ(2, train_get_capacity("12627", RRS_CLASS_SLEEPER));
}

TEST_F(SecurityValidation, Sec002PassengerCannotReadOrCancelAnotherPassengersBooking) {
    rrs_booking_t mine, peek;
    ASSERT_EQ(RRS_OK, booking_create(&alice, "12627", RRS_CLASS_SLEEPER, 236.0, "k1", &mine));
    EXPECT_EQ(ERR_FORBIDDEN, booking_get(&bob, mine.pnr, &peek));
    EXPECT_EQ(ERR_FORBIDDEN, booking_cancel(&bob, mine.pnr));
    ASSERT_EQ(RRS_OK, booking_get(&alice, mine.pnr, &peek));
    EXPECT_EQ(RRS_BOOKING_CONFIRMED, peek.status);
}

/* SEC-003: overlong strings, special characters and negative numbers in the
 * search and booking fields are rejected and change nothing. (Registration,
 * login and profile fields are fuzzed in tests/security/test_input_fuzz.cpp.) */
TEST_F(SecurityValidation, Sec003SearchRejectsMalformedInput) {
    rrs_route_list_t out;
    const char* bad_stations[] = {"", "S", "SBCMASNDLS", "SB;C", "S\nC", "sbc", "'OR1", "S C"};
    for (const char* s : bad_stations) {
        EXPECT_EQ(ERR_INVALID_INPUT, search_routes(s, "MAS", "2026-11-20", nullptr, &out)) << s;
        EXPECT_EQ(ERR_INVALID_INPUT, search_routes("SBC", s, "2026-11-20", nullptr, &out)) << s;
    }
    const char* bad_dates[] = {"", "2026-02-30", "2026-13-01", "20-11-2026", "2026-11-20;", "9999999999"};
    for (const char* d : bad_dates)
        EXPECT_EQ(ERR_INVALID_INPUT, search_routes("SBC", "MAS", d, nullptr, &out)) << d;
    rrs_search_opts_t o = search_default_opts();
    o.max_legs = -1;
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("SBC", "MAS", "2026-11-20", &o, &out));
    o = search_default_opts();
    o.min_transfer_minutes = -30;
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("SBC", "MAS", "2026-11-20", &o, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes(nullptr, "MAS", "2026-11-20", nullptr, &out));
}

TEST_F(SecurityValidation, Sec003BookingRejectsMalformedInput) {
    rrs_booking_t b;
    std::string long_key(200, 'k');
    EXPECT_EQ(ERR_INVALID_INPUT, booking_create(&alice, "12627;", RRS_CLASS_SLEEPER, 236.0, nullptr, &b));
    EXPECT_EQ(ERR_INVALID_INPUT, booking_create(&alice, "12627123456789", RRS_CLASS_SLEEPER, 236.0, nullptr, &b));
    EXPECT_EQ(ERR_INVALID_INPUT, booking_create(&alice, "", RRS_CLASS_SLEEPER, 236.0, nullptr, &b));
    EXPECT_EQ(ERR_INVALID_INPUT, booking_create(&alice, "12627", RRS_CLASS_SLEEPER, -236.0, nullptr, &b));
    EXPECT_EQ(ERR_INVALID_INPUT, booking_create(&alice, "12627", RRS_CLASS_SLEEPER, 0.0, nullptr, &b));
    EXPECT_EQ(ERR_INVALID_INPUT, booking_create(&alice, "12627", static_cast<rrs_class_t>(-1), 236.0, nullptr, &b));
    EXPECT_EQ(ERR_INVALID_INPUT, booking_create(&alice, "12627", static_cast<rrs_class_t>(99), 236.0, nullptr, &b));
    EXPECT_EQ(ERR_INVALID_INPUT, booking_create(&alice, "12627", RRS_CLASS_SLEEPER, 236.0, long_key.c_str(), &b));
    EXPECT_EQ(ERR_INVALID_INPUT, booking_create(&alice, nullptr, RRS_CLASS_SLEEPER, 236.0, nullptr, &b));
    EXPECT_EQ(0, booking_count_confirmed("12627", RRS_CLASS_SLEEPER));
    EXPECT_EQ(0, booking_count_waitlisted("12627", RRS_CLASS_SLEEPER));
}

TEST_F(SecurityValidation, Sec003AdminTrainInputIsValidated) {
    rrs_train_t t = train("55555", -5);                     /* negative capacity */
    EXPECT_EQ(ERR_INVALID_INPUT, train_create(&admin, &t));
    t = train("55555", 5);
    t.halts[1].distance_km = -100;                          /* negative distance */
    EXPECT_EQ(ERR_INVALID_INPUT, train_create(&admin, &t));
    t = train("55;55", 5);                                  /* bad id */
    EXPECT_EQ(ERR_INVALID_INPUT, train_create(&admin, &t));
    t = train("55555", 5);
    t.halts[1].arrive_min = t.halts[1].depart_min = 2147483000;   /* overflow attempt */
    EXPECT_EQ(ERR_INVALID_INPUT, train_create(&admin, &t));
    EXPECT_EQ(1, inventory_train_count_for_tests());
}

/* SEC-004: reusing an expired or invalid session token gets ERR_SESSION_EXPIRED
 * on every protected call in inventory and booking. (Auth's own calls are
 * covered in tests/security/test_session_security.cpp.) */
TEST_F(SecurityValidation, Sec004ExpiredAndInvalidTokensAreRejectedEverywhere) {
    rrs_booking_t booked;
    ASSERT_EQ(RRS_OK, booking_create(&alice, "12627", RRS_CLASS_SLEEPER, 236.0, "k1", &booked));

    rrs_session_t expired, forged = alice, logged_out;
    const time_t t0 = 1800000000;
    rrs::session_set_fixed_time_for_tests(t0);
    ASSERT_EQ(RRS_OK, auth_login("admin@rrs.in", "Adm1nPassw0rd", &expired));
    rrs::session_set_fixed_time_for_tests(t0 + rrs::kSessionTtlSeconds + 1);
    std::memset(forged.token, 'f', RRS_TOKEN_LEN - 1);
    ASSERT_EQ(RRS_OK, auth_login("bob@rrs.in", "Passw0rdOK", &logged_out));
    ASSERT_EQ(RRS_OK, auth_logout(&logged_out));

    for (const rrs_session_t* s : {&expired, &forged, &logged_out}) {
        rrs_train_t t = train("77777", 5), got;
        rrs_booking_t b;
        EXPECT_EQ(ERR_SESSION_EXPIRED, train_create(s, &t));
        EXPECT_EQ(ERR_SESSION_EXPIRED, train_update(s, &t));
        EXPECT_EQ(ERR_SESSION_EXPIRED, train_retire(s, "12627"));
        EXPECT_EQ(ERR_SESSION_EXPIRED, train_get(s, "12627", &got));
        EXPECT_EQ(ERR_SESSION_EXPIRED, booking_create(s, "12627", RRS_CLASS_SLEEPER, 236.0, nullptr, &b));
        EXPECT_EQ(ERR_SESSION_EXPIRED, booking_get(s, booked.pnr, &b));
        EXPECT_EQ(ERR_SESSION_EXPIRED, booking_cancel(s, booked.pnr));
    }
    EXPECT_EQ(1, booking_count_confirmed("12627", RRS_CLASS_SLEEPER));
    EXPECT_EQ(1, train_is_bookable("12627"));
}

/* SEC-005: booking records keep only a payment success flag; a declined mock
 * payment stores nothing at all. */
TEST_F(SecurityValidation, Sec005OnlyAPaymentFlagIsKept) {
    rrs_booking_t ok, declined;
    ASSERT_EQ(RRS_OK, booking_create(&alice, "12627", RRS_CLASS_SLEEPER, 236.0, "k1", &ok));
    EXPECT_EQ(1, ok.payment_success);

    booking_set_mock_payment_result(0);
    EXPECT_EQ(ERR_PAYMENT_FAILED, booking_create(&bob, "12627", RRS_CLASS_SLEEPER, 236.0, "k2", &declined));
    booking_set_mock_payment_result(-1);
    EXPECT_EQ(0, declined.pnr[0]);
    rrs_booking_t list[4];
    int n = -1;
    ASSERT_EQ(RRS_OK, booking_list_by_passenger(&bob, list, 4, &n));
    EXPECT_EQ(0, n);
}

/* SEC-006: three failed logins and an admin action appear in the audit log;
 * so does a passenger's denied attempt at an admin function. */
TEST_F(SecurityValidation, Sec006FailedLoginsAndAdminActionsAreAudited) {
    rrs_session_t s;
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(ERR_AUTH_FAILED, auth_login("alice@rrs.in", "WrongPass1", &s));
    rrs_train_t t = train("22691", 10);
    ASSERT_EQ(RRS_OK, train_create(&admin, &t));
    t = train("99999", 10);
    ASSERT_EQ(ERR_FORBIDDEN, train_create(&alice, &t));

    std::vector<std::string> log = audit_lines();
    EXPECT_EQ(3, count_lines_with(log, "event=LOGIN_FAILED", "user=alice@rrs.in"));
    EXPECT_EQ(1, count_lines_with(log, "event=ADMIN_ACTION", "fn=train_create"));
    EXPECT_EQ(1, count_lines_with(log, "event=ACCESS_DENIED", "fn=train_create"));
    for (const std::string& line : log) {
        EXPECT_EQ(std::string::npos, line.find("WrongPass1"));
        EXPECT_EQ(std::string::npos, line.find(admin.token));
    }
}
