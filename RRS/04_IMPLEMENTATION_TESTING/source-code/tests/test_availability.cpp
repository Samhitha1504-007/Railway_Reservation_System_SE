/*
 * NFR-003 Availability: 99% of requests succeed during the evaluation window.
 *
 * A soak run of mixed passenger and admin requests against the in-process
 * modules. Every request that is valid must succeed, so the success rate of
 * valid requests must stay at or above 99%, and the system must keep serving
 * after bad input and after a rejected payment.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>

#include "auth.h"
#include "auth_internal.h"
#include "booking.h"
#include "booking_internal.h"
#include "inventory.h"
#include "inventory_internal.h"
#include "search.h"

namespace {

rrs_train_t make_train(const char* id, int ac3) {
    rrs_train_t t;
    std::memset(&t, 0, sizeof t);
    std::strncpy(t.train_id, id, RRS_TRAIN_ID_LEN - 1);
    const char* st[2] = {"NDLS", "LKO"};
    const int tm[2] = {480, 900};
    const int km[2] = {0, 512};
    t.halt_count = 2;
    for (int i = 0; i < 2; ++i) {
        std::strncpy(t.halts[i].station, st[i], RRS_STATION_CODE_LEN - 1);
        t.halts[i].arrive_min = tm[i];
        t.halts[i].depart_min = tm[i];
        t.halts[i].distance_km = km[i];
    }
    t.capacity[RRS_CLASS_AC3] = ac3;
    return t;
}

class AvailabilityTest : public ::testing::Test {
protected:
    rrs_session_t admin, user;
    void SetUp() override {
        auth_reset_for_tests();
        inventory_reset_for_tests();
        booking_clear();
        booking_init();
        booking_set_mock_payment_result(-1);
        ASSERT_EQ(auth_create_user_for_tests("admin@rrs.test", "Admin1234", "Admin", RRS_ROLE_ADMIN), RRS_OK);
        ASSERT_EQ(auth_login("admin@rrs.test", "Admin1234", &admin), RRS_OK);
        ASSERT_EQ(auth_register("u@rrs.test", "Pass1234", "U"), RRS_OK);
        ASSERT_EQ(auth_login("u@rrs.test", "Pass1234", &user), RRS_OK);
        rrs_train_t t = make_train("EXP101", 5000);
        ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    }
    void TearDown() override {
        booking_set_mock_payment_result(-1);
        booking_clear();
        inventory_reset_for_tests();
        auth_reset_for_tests();
    }
};

}  // namespace

TEST_F(AvailabilityTest, ValidRequestsSucceedAtLeast99PercentOverASoakRun) {
    int total = 0, ok = 0;
    for (int i = 0; i < 2000; ++i) {
        rrs_status_t st;
        switch (i % 4) {
            case 0: {
                rrs_route_list_t r;
                st = search_routes("NDLS", "LKO", "2026-11-02", nullptr, &r);
                break;
            }
            case 1: {
                rrs_booking_t b;
                char key[32];
                std::snprintf(key, sizeof key, "soak-%d", i);
                st = booking_create(&user, "EXP101", RRS_CLASS_AC3, 900.0, key, &b);
                if (st == RRS_OK) {
                    booking_cancel(&user, b.pnr);
                }
                break;
            }
            case 2: {
                rrs_train_t t;
                st = train_get(&admin, "EXP101", &t);
                break;
            }
            default: {
                rrs_booking_t list[8];
                int n = 0;
                st = booking_list_by_passenger(&user, list, 8, &n);
                break;
            }
        }
        ++total;
        if (st == RRS_OK) ++ok;
    }
    EXPECT_GE(ok * 100.0 / total, 99.0) << ok << " of " << total << " valid requests succeeded";
}

TEST_F(AvailabilityTest, ServiceKeepsRunningAfterBadInputAndFailedPayment) {
    rrs_booking_t b;
    EXPECT_EQ(booking_create(nullptr, "EXP101", RRS_CLASS_AC3, 900.0, "x", &b), ERR_INVALID_INPUT);
    EXPECT_EQ(booking_create(&user, "NOPE", RRS_CLASS_AC3, 900.0, "x", &b), ERR_NOT_FOUND);
    booking_set_mock_payment_result(0);
    EXPECT_EQ(booking_create(&user, "EXP101", RRS_CLASS_AC3, 900.0, "y", &b), ERR_PAYMENT_FAILED);
    booking_set_mock_payment_result(-1);

    EXPECT_EQ(booking_create(&user, "EXP101", RRS_CLASS_AC3, 900.0, "z", &b), RRS_OK);
    rrs_route_list_t r;
    EXPECT_EQ(search_routes("NDLS", "LKO", "2026-11-02", nullptr, &r), RRS_OK);
}

TEST_F(AvailabilityTest, ServiceRecoversAfterTrainRetireAndRestock) {
    rrs_booking_t b;
    ASSERT_EQ(train_retire(&admin, "EXP101"), RRS_OK);
    EXPECT_EQ(booking_create(&user, "EXP101", RRS_CLASS_AC3, 900.0, "r1", &b), ERR_NOT_FOUND);

    rrs_train_t t = make_train("EXP202", 10);
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    EXPECT_EQ(booking_create(&user, "EXP202", RRS_CLASS_AC3, 900.0, "r2", &b), RRS_OK);
}
