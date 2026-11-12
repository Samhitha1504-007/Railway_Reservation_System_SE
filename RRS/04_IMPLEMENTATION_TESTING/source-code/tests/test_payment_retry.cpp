/*
 * NFR-005 Reliability: a payment-timeout retry never creates a second booking.
 *
 * A passenger's client may time out while a payment is in flight and send the
 * same request again with the same idempotency key. Whatever happened to the
 * first attempt, the retry must end with exactly one booking and one seat.
 */
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "auth.h"
#include "auth_internal.h"
#include "booking.h"
#include "booking_internal.h"
#include "inventory.h"
#include "inventory_internal.h"

namespace {

int g_gateway_calls = 0;
int g_gateway_results_idx = 0;
std::vector<int> g_gateway_results;

/* Scripted gateway: returns the next result in the list (1 = paid, 0 = timed out). */
int scripted_gateway(const char*, double) {
    ++g_gateway_calls;
    if (g_gateway_results_idx < static_cast<int>(g_gateway_results.size()))
        return g_gateway_results[g_gateway_results_idx++];
    return 1;
}

void script_gateway(std::vector<int> results) {
    g_gateway_calls = 0;
    g_gateway_results_idx = 0;
    g_gateway_results = std::move(results);
    booking_set_mock_payment_result(-1); /* use the gateway, not the fixed override */
    booking_set_payment_gateway(scripted_gateway);
}

rrs_train_t make_train(const char* id, int sleeper, int ac3, int ac2) {
    rrs_train_t t;
    std::memset(&t, 0, sizeof t);
    std::strncpy(t.train_id, id, RRS_TRAIN_ID_LEN - 1);
    const char* stations[3] = {"NDLS", "CNB", "LKO"};
    const int times[3] = {480, 720, 840};
    const int kms[3] = {0, 440, 510};
    t.halt_count = 3;
    for (int i = 0; i < 3; ++i) {
        std::strncpy(t.halts[i].station, stations[i], RRS_STATION_CODE_LEN - 1);
        t.halts[i].arrive_min = times[i];
        t.halts[i].depart_min = times[i];
        t.halts[i].distance_km = kms[i];
    }
    t.capacity[RRS_CLASS_SLEEPER] = sleeper;
    t.capacity[RRS_CLASS_AC3] = ac3;
    t.capacity[RRS_CLASS_AC2] = ac2;
    return t;
}

class PaymentRetryTest : public ::testing::Test {
protected:
    rrs_session_t admin;
    rrs_session_t alice;
    rrs_session_t bob;

    void SetUp() override {
        auth_reset_for_tests();
        inventory_reset_for_tests();
        booking_clear();
        booking_init();

        ASSERT_EQ(auth_create_user_for_tests("admin@rrs.test", "Admin1234", "Admin", RRS_ROLE_ADMIN), RRS_OK);
        ASSERT_EQ(auth_login("admin@rrs.test", "Admin1234", &admin), RRS_OK);
        ASSERT_EQ(auth_register("alice@rrs.test", "Pass1234", "Alice"), RRS_OK);
        ASSERT_EQ(auth_login("alice@rrs.test", "Pass1234", &alice), RRS_OK);
        ASSERT_EQ(auth_register("bob@rrs.test", "Pass1234", "Bob"), RRS_OK);
        ASSERT_EQ(auth_login("bob@rrs.test", "Pass1234", &bob), RRS_OK);

        rrs_train_t t = make_train("EXP101", 100, 2, 20);
        ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    }

    void TearDown() override {
        booking_set_payment_gateway(nullptr);
        booking_set_mock_payment_result(-1);
        booking_clear();
        inventory_reset_for_tests();
        auth_reset_for_tests();
    }

    int bookings_of(const rrs_session_t* s) {
        rrs_booking_t list[16];
        int n = 0;
        EXPECT_EQ(booking_list_by_passenger(s, list, 16, &n), RRS_OK);
        return n;
    }
};

}  // namespace

/* Payment times out before the charge goes through: nothing is created, and the
 * retry with the same key creates exactly one booking. */
TEST_F(PaymentRetryTest, TimeoutBeforeChargeLeavesNoBookingThenRetrySucceedsOnce) {
    script_gateway({0, 1});
    rrs_booking_t first, retry;

    EXPECT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, "key-timeout-1", &first),
              ERR_PAYMENT_FAILED);
    EXPECT_EQ(bookings_of(&alice), 0);
    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 0);

    EXPECT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, "key-timeout-1", &retry), RRS_OK);
    EXPECT_EQ(retry.status, RRS_BOOKING_CONFIRMED);
    EXPECT_EQ(bookings_of(&alice), 1);
    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 1);
}

/* The charge went through but the client timed out waiting for the reply and
 * retries: the retry returns the same booking and the gateway is not charged again. */
TEST_F(PaymentRetryTest, RetryAfterSuccessfulChargeReturnsSamePnrWithoutSecondCharge) {
    script_gateway({1, 1, 1});
    rrs_booking_t first, retry;

    ASSERT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, "key-timeout-2", &first), RRS_OK);
    ASSERT_EQ(g_gateway_calls, 1);

    ASSERT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, "key-timeout-2", &retry), RRS_OK);
    EXPECT_STREQ(retry.pnr, first.pnr);
    EXPECT_EQ(g_gateway_calls, 1) << "a retry must not charge the passenger twice";
    EXPECT_EQ(bookings_of(&alice), 1);
    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 1);
}

/* Several timeouts in a row, then success, then more retries: still one booking. */
TEST_F(PaymentRetryTest, RepeatedTimeoutsThenSuccessThenRetriesStayAtOneBooking) {
    script_gateway({0, 0, 0, 1});
    rrs_booking_t b;
    const char* key = "key-timeout-3";

    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, key, &b), ERR_PAYMENT_FAILED);
        EXPECT_EQ(bookings_of(&alice), 0);
    }
    ASSERT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, key, &b), RRS_OK);
    char pnr[RRS_PNR_LEN];
    std::strncpy(pnr, b.pnr, RRS_PNR_LEN);

    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, key, &b), RRS_OK);
        EXPECT_STREQ(b.pnr, pnr);
    }
    EXPECT_EQ(bookings_of(&alice), 1);
    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 1);
    EXPECT_EQ(g_gateway_calls, 4);
}

/* A retried request that landed on the waitlist must not join the waitlist twice. */
TEST_F(PaymentRetryTest, RetryOfWaitlistedBookingDoesNotDuplicateWaitlistEntry) {
    script_gateway({});
    rrs_booking_t b, w, retry;

    /* AC3 capacity is 2: fill it, then alice waitlists. */
    ASSERT_EQ(booking_create(&bob, "EXP101", RRS_CLASS_AC3, 850.0, "bob-1", &b), RRS_OK);
    ASSERT_EQ(booking_create(&bob, "EXP101", RRS_CLASS_AC3, 850.0, "bob-2", &b), RRS_OK);
    ASSERT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, "key-wait", &w), RRS_OK);
    ASSERT_EQ(w.status, RRS_BOOKING_WAITLISTED);
    ASSERT_EQ(booking_count_waitlisted("EXP101", RRS_CLASS_AC3), 1);

    ASSERT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, "key-wait", &retry), RRS_OK);
    EXPECT_STREQ(retry.pnr, w.pnr);
    EXPECT_EQ(retry.waitlist_position, w.waitlist_position);
    EXPECT_EQ(booking_count_waitlisted("EXP101", RRS_CLASS_AC3), 1);
    EXPECT_EQ(bookings_of(&alice), 1);
}

/* The same key used by two different passengers is two separate bookings. */
TEST_F(PaymentRetryTest, SameKeyFromDifferentPassengersDoesNotCollide) {
    script_gateway({});
    rrs_booking_t a, b;

    ASSERT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, "shared-key", &a), RRS_OK);
    ASSERT_EQ(booking_create(&bob, "EXP101", RRS_CLASS_AC3, 850.0, "shared-key", &b), RRS_OK);
    EXPECT_STRNE(a.pnr, b.pnr);
    EXPECT_EQ(bookings_of(&alice), 1);
    EXPECT_EQ(bookings_of(&bob), 1);
}

/* A cancelled booking keeps its key: a late retry must not silently rebook. */
TEST_F(PaymentRetryTest, RetryAfterCancelDoesNotCreateNewActiveBooking) {
    script_gateway({});
    rrs_booking_t first, retry;

    ASSERT_EQ(booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, "key-cancel", &first), RRS_OK);
    ASSERT_EQ(booking_cancel(&alice, first.pnr), RRS_OK);
    ASSERT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 0);

    (void)booking_create(&alice, "EXP101", RRS_CLASS_AC3, 850.0, "key-cancel", &retry);
    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 0)
        << "a stale retry must not consume a seat again";
}
