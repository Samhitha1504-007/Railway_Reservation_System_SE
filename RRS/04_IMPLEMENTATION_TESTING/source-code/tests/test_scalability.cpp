/*
 * NFR-008: the route graph and the booking / waitlist structures support at
 * least 500 trains and 10,000 bookings without redesign.
 */
#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <string>

#include "auth.h"
#include "auth_internal.h"
#include "booking.h"
#include "inventory.h"
#include "inventory_internal.h"
#include "search.h"

namespace {

const int kTrains = 500;
const int kBookings = 10000;

rrs_train_t one_hop_train(const std::string& id, const char* from, const char* to, int seats) {
    rrs_train_t t;
    std::memset(&t, 0, sizeof t);
    std::snprintf(t.train_id, sizeof t.train_id, "%s", id.c_str());
    t.halt_count = 2;
    std::snprintf(t.halts[0].station, RRS_STATION_CODE_LEN, "%s", from);
    std::snprintf(t.halts[1].station, RRS_STATION_CODE_LEN, "%s", to);
    t.halts[1].arrive_min = t.halts[1].depart_min = 120;
    t.halts[1].distance_km = 100;
    t.capacity[RRS_CLASS_SLEEPER] = seats;
    return t;
}

double seconds_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

class ScalabilityTest : public ::testing::Test {
protected:
    rrs_session_t admin, passenger;
    void SetUp() override {
        auth_reset_for_tests();
        inventory_reset_for_tests();
        booking_clear();
        booking_init();
        ASSERT_EQ(RRS_OK, auth_create_user_for_tests("admin@rrs.in", "Adm1nPassw0rd", "Admin",
                                                     RRS_ROLE_ADMIN));
        ASSERT_EQ(RRS_OK, auth_register("rider@rrs.in", "Passw0rdOK", "Rider"));
        ASSERT_EQ(RRS_OK, auth_login("admin@rrs.in", "Adm1nPassw0rd", &admin));
        ASSERT_EQ(RRS_OK, auth_login("rider@rrs.in", "Passw0rdOK", &passenger));
    }
    void TearDown() override {
        booking_clear();
        inventory_reset_for_tests();
    }
};

TEST_F(ScalabilityTest, FiveHundredTrainsAreStoredAndSearchable) {
    for (int t = 0; t < kTrains; ++t) {
        std::string from = "S" + std::to_string(t % 100), to = "D" + std::to_string(t % 100);
        rrs_train_t train = one_hop_train("T" + std::to_string(t), from.c_str(), to.c_str(), 10);
        ASSERT_EQ(RRS_OK, train_create(&admin, &train)) << t;
    }
    EXPECT_EQ(kTrains, inventory_train_count_for_tests());

    /* five trains run S7 -> D7; every one of them must be found */
    rrs_route_list_t out;
    ASSERT_EQ(RRS_OK, search_routes("S7", "D7", "2026-11-20", nullptr, &out));
    EXPECT_EQ(kTrains / 100, out.count);
}

TEST_F(ScalabilityTest, TenThousandBookingsAcrossFiveHundredTrains) {
    for (int t = 0; t < kTrains; ++t) {
        rrs_train_t train = one_hop_train("T" + std::to_string(t), "A", "B", 15);
        ASSERT_EQ(RRS_OK, train_create(&admin, &train));
    }

    auto t0 = std::chrono::steady_clock::now();
    int confirmed = 0, waitlisted = 0;
    for (int i = 0; i < kBookings; ++i) {
        std::string id = "T" + std::to_string(i % kTrains);
        rrs_booking_t b;
        ASSERT_EQ(RRS_OK, booking_create(&passenger, id.c_str(), RRS_CLASS_SLEEPER, 100.0, nullptr, &b))
            << "booking " << i;
        (b.status == RRS_BOOKING_CONFIRMED ? confirmed : waitlisted)++;
    }
    double s = seconds_since(t0);
    std::printf("  10,000 bookings over 500 trains: %d confirmed, %d waitlisted, %.2f s\n",
                confirmed, waitlisted, s);

    /* 20 per train, 15 seats each */
    EXPECT_EQ(kTrains * 15, confirmed);
    EXPECT_EQ(kTrains * 5, waitlisted);
    EXPECT_EQ(15, booking_count_confirmed("T42", RRS_CLASS_SLEEPER));
    EXPECT_EQ(5, booking_count_waitlisted("T42", RRS_CLASS_SLEEPER));
    EXPECT_LT(s, 60.0);
}

TEST_F(ScalabilityTest, WaitlistHoldsTenThousandMinusOneForASingleSeat) {
    rrs_train_t train = one_hop_train("ONE", "A", "B", 1);
    ASSERT_EQ(RRS_OK, train_create(&admin, &train));

    rrs_booking_t first, b;
    ASSERT_EQ(RRS_OK, booking_create(&passenger, "ONE", RRS_CLASS_SLEEPER, 100.0, nullptr, &first));
    ASSERT_EQ(RRS_BOOKING_CONFIRMED, first.status);
    for (int i = 1; i < kBookings; ++i) {
        ASSERT_EQ(RRS_OK, booking_create(&passenger, "ONE", RRS_CLASS_SLEEPER, 100.0, nullptr, &b))
            << "booking " << i;
        ASSERT_EQ(RRS_BOOKING_WAITLISTED, b.status);
        ASSERT_EQ(i, b.waitlist_position);
    }
    EXPECT_EQ(1, booking_count_confirmed("ONE", RRS_CLASS_SLEEPER));
    EXPECT_EQ(kBookings - 1, booking_count_waitlisted("ONE", RRS_CLASS_SLEEPER));

    /* cancelling the one seat promotes the head of a 9,999-long queue */
    ASSERT_EQ(RRS_OK, booking_cancel(&passenger, first.pnr));
    EXPECT_EQ(1, booking_count_confirmed("ONE", RRS_CLASS_SLEEPER));
    EXPECT_EQ(kBookings - 2, booking_count_waitlisted("ONE", RRS_CLASS_SLEEPER));
}
