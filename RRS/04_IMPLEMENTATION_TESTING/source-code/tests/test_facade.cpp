#include <gtest/gtest.h>

#include <cstring>

#include "auth.h"
#include "auth_internal.h"
#include "booking.h"
#include "facade.h"
#include "inventory.h"
#include "inventory_internal.h"

class FacadeTest : public ::testing::Test {
protected:
    rrs_session_t user;

    void SetUp() override {
        auth_reset_for_tests();
        inventory_reset_for_tests();
        booking_clear();
        ASSERT_EQ(facade_init(""), RRS_OK);
        ASSERT_EQ(facade_seed_demo(), RRS_OK);
        ASSERT_EQ(auth_register("riya@example.com", "Passw0rdOK", "Riya"), RRS_OK);
        ASSERT_EQ(auth_login("riya@example.com", "Passw0rdOK", &user), RRS_OK);
    }
    void TearDown() override {
        booking_set_mock_payment_result(-1);
        booking_clear();
        inventory_reset_for_tests();
        auth_reset_for_tests();
    }
};

TEST_F(FacadeTest, SearchBookAndCancelEndToEnd) {
    rrs_option_list_t list;
    ASSERT_EQ(facade_search(&user, "SBC", "NDLS", "2026-11-02", RRS_CLASS_AC3,
                            RRS_CONCESSION_NONE, RRS_SORT_FARE, nullptr, &list), RRS_OK);
    ASSERT_GE(list.count, 1);

    rrs_booking_t b[RRS_MAX_LEGS];
    int n = 0;
    ASSERT_EQ(facade_book(&user, &list.options[0].route, RRS_CLASS_AC3, RRS_CONCESSION_NONE,
                          "t-1", b, &n), RRS_OK);
    ASSERT_EQ(n, list.options[0].route.leg_count);
    EXPECT_EQ(b[0].status, RRS_BOOKING_CONFIRMED);

    rrs_booking_t mine[8];
    int m = 0;
    ASSERT_EQ(facade_my_bookings(&user, mine, 8, &m), RRS_OK);
    EXPECT_EQ(m, n);
    EXPECT_EQ(facade_cancel(&user, b[0].pnr), RRS_OK);
}

TEST_F(FacadeTest, FareComesFromFareCalculateNotFromTheCaller) {
    rrs_option_list_t list;
    ASSERT_EQ(facade_search(&user, "NDLS", "LKO", "2026-11-02", RRS_CLASS_AC3,
                            RRS_CONCESSION_NONE, RRS_SORT_TIME, nullptr, &list), RRS_OK);
    ASSERT_EQ(list.options[0].route.leg_count, 1);
    rrs_booking_t b[RRS_MAX_LEGS];
    int n = 0;
    ASSERT_EQ(facade_book(&user, &list.options[0].route, RRS_CLASS_AC3, RRS_CONCESSION_NONE,
                          "t-2", b, &n), RRS_OK);
    EXPECT_DOUBLE_EQ(b[0].fare_amount, list.options[0].fare);
}

TEST_F(FacadeTest, RetryWithSameKeyDoesNotDuplicate) {
    rrs_option_list_t list;
    ASSERT_EQ(facade_search(&user, "NDLS", "LKO", "2026-11-02", RRS_CLASS_AC3,
                            RRS_CONCESSION_NONE, RRS_SORT_TIME, nullptr, &list), RRS_OK);
    rrs_booking_t a[RRS_MAX_LEGS], c[RRS_MAX_LEGS];
    int na = 0, nc = 0;
    ASSERT_EQ(facade_book(&user, &list.options[0].route, RRS_CLASS_AC3, RRS_CONCESSION_NONE,
                          "same", a, &na), RRS_OK);
    ASSERT_EQ(facade_book(&user, &list.options[0].route, RRS_CLASS_AC3, RRS_CONCESSION_NONE,
                          "same", c, &nc), RRS_OK);
    EXPECT_STREQ(a[0].pnr, c[0].pnr);
    rrs_booking_t mine[8];
    int m = 0;
    ASSERT_EQ(facade_my_bookings(&user, mine, 8, &m), RRS_OK);
    EXPECT_EQ(m, 1);
}

TEST_F(FacadeTest, MultiLegBookingIsRolledBackWhenALaterLegFails) {
    rrs_option_list_t list;
    ASSERT_EQ(facade_search(&user, "SBC", "LKO", "2026-11-02", RRS_CLASS_AC3,
                            RRS_CONCESSION_NONE, RRS_SORT_TIME, nullptr, &list), RRS_OK);
    ASSERT_EQ(list.options[0].route.leg_count, 2);
    /* Second leg's train is retired between search and booking. */
    rrs_session_t admin;
    ASSERT_EQ(auth_login("admin@rrs.in", "Adm1nPassw0rd", &admin), RRS_OK);
    ASSERT_EQ(train_retire(&admin, list.options[0].route.legs[1].train_id), RRS_OK);

    rrs_booking_t b[RRS_MAX_LEGS];
    int n = 0;
    EXPECT_NE(facade_book(&user, &list.options[0].route, RRS_CLASS_AC3, RRS_CONCESSION_NONE,
                          "t-3", b, &n), RRS_OK);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(booking_count_confirmed(list.options[0].route.legs[0].train_id, RRS_CLASS_AC3), 0);
}

TEST_F(FacadeTest, FailedPaymentBooksNothing) {
    rrs_option_list_t list;
    ASSERT_EQ(facade_search(&user, "NDLS", "LKO", "2026-11-02", RRS_CLASS_AC3,
                            RRS_CONCESSION_NONE, RRS_SORT_TIME, nullptr, &list), RRS_OK);
    booking_set_mock_payment_result(0);
    rrs_booking_t b[RRS_MAX_LEGS];
    int n = 0;
    EXPECT_EQ(facade_book(&user, &list.options[0].route, RRS_CLASS_AC3, RRS_CONCESSION_NONE,
                          "t-4", b, &n), ERR_PAYMENT_FAILED);
    EXPECT_EQ(n, 0);
}

TEST_F(FacadeTest, RejectsMissingOrBadSessionAndInput) {
    rrs_option_list_t list;
    EXPECT_EQ(facade_search(nullptr, "SBC", "NDLS", "2026-11-02", RRS_CLASS_AC3,
                            RRS_CONCESSION_NONE, RRS_SORT_TIME, nullptr, &list),
              ERR_SESSION_EXPIRED);
    rrs_session_t forged = user;
    std::strcpy(forged.token, "not-a-real-token");
    EXPECT_NE(facade_search(&forged, "SBC", "NDLS", "2026-11-02", RRS_CLASS_AC3,
                            RRS_CONCESSION_NONE, RRS_SORT_TIME, nullptr, &list), RRS_OK);
    EXPECT_EQ(facade_search(&user, "SBC", "SBC", "2026-11-02", RRS_CLASS_AC3,
                            RRS_CONCESSION_NONE, RRS_SORT_TIME, nullptr, &list),
              ERR_INVALID_INPUT);

    rrs_booking_t b[RRS_MAX_LEGS];
    int n = 0;
    rrs_route_t empty;
    std::memset(&empty, 0, sizeof empty);
    EXPECT_EQ(facade_book(&user, &empty, RRS_CLASS_AC3, RRS_CONCESSION_NONE, "k", b, &n),
              ERR_INVALID_INPUT);
    EXPECT_EQ(facade_book(&user, &empty, RRS_CLASS_AC3, RRS_CONCESSION_NONE, "", b, &n),
              ERR_INVALID_INPUT);
}
