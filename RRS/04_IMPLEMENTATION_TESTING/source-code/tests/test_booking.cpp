#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "auth.h"
#include "auth_internal.h"
#include "booking.h"
#include "booking_internal.h"
#include "inventory.h"
#include "inventory_internal.h"
#include "search.h"

namespace {

rrs_halt_t make_halt(const char* station, int arrive, int depart, int km) {
    rrs_halt_t h;
    std::memset(&h, 0, sizeof(h));
    std::strncpy(h.station, station, RRS_STATION_CODE_LEN - 1);
    h.arrive_min = arrive;
    h.depart_min = depart;
    h.distance_km = km;
    return h;
}

rrs_train_t make_train(const char* id, int sleeper = 100, int ac3 = 50, int ac2 = 20) {
    rrs_train_t t;
    std::memset(&t, 0, sizeof(t));
    std::strncpy(t.train_id, id, RRS_TRAIN_ID_LEN - 1);
    std::vector<rrs_halt_t> halts = {
        make_halt("NDLS", 480, 480, 0),
        make_halt("CNB", 720, 725, 440),
        make_halt("LKO", 840, 840, 510)
    };
    t.halt_count = static_cast<int>(halts.size());
    std::memcpy(t.halts, halts.data(), halts.size() * sizeof(rrs_halt_t));
    t.capacity[RRS_CLASS_SLEEPER] = sleeper;
    t.capacity[RRS_CLASS_AC3] = ac3;
    t.capacity[RRS_CLASS_AC2] = ac2;
    return t;
}

class BookingTest : public ::testing::Test {
protected:
    rrs_session_t admin_session;
    rrs_session_t passenger_session;

    void SetUp() override {
        auth_reset_for_tests();
        inventory_reset_for_tests();
        booking_clear();
        booking_init();

        /* Set up admin */
        ASSERT_EQ(auth_create_user_for_tests("admin@rrs.test", "Admin1234", "Admin", RRS_ROLE_ADMIN), RRS_OK);
        ASSERT_EQ(auth_login("admin@rrs.test", "Admin1234", &admin_session), RRS_OK);

        /* Set up passenger */
        ASSERT_EQ(auth_register("passenger@rrs.test", "Pass1234", "Test Passenger"), RRS_OK);
        ASSERT_EQ(auth_login("passenger@rrs.test", "Pass1234", &passenger_session), RRS_OK);

        /* Create a valid active train for booking */
        rrs_train_t t = make_train("EXP101", 100, 50, 20);
        ASSERT_EQ(train_create(&admin_session, &t), RRS_OK);
    }

    void TearDown() override {
        booking_clear();
        inventory_reset_for_tests();
        auth_reset_for_tests();
    }
};

/* ========================================================================= */
/* FR-018: Create Booking and Generate Unique PNR                            */
/* ========================================================================= */

TEST_F(BookingTest, CreateBookingSuccessfulPaymentGeneratesConfirmedPNR) {
    rrs_booking_t booking;
    rrs_status_t st = booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 850.0,
                                     "txn-001", &booking);
    EXPECT_EQ(st, RRS_OK);
    EXPECT_EQ(booking.status, RRS_BOOKING_CONFIRMED);
    EXPECT_EQ(booking.waitlist_position, 0);
    EXPECT_EQ(booking.payment_success, 1);
    EXPECT_DOUBLE_EQ(booking.fare_amount, 850.0);
    EXPECT_STREQ(booking.passenger_id, passenger_session.user_id);
    EXPECT_STREQ(booking.train_id, "EXP101");
    EXPECT_EQ(booking.travel_class, RRS_CLASS_AC3);

    /* Verify 12-char PNR format */
    EXPECT_EQ(std::strlen(booking.pnr), 12u);
    EXPECT_EQ(std::strncmp(booking.pnr, "PNR", 3), 0);

    /* Confirmed count must be 1 */
    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 1);

    /* Retrieve booking and verify matches */
    rrs_booking_t fetched;
    st = booking_get(&passenger_session, booking.pnr, &fetched);
    EXPECT_EQ(st, RRS_OK);
    EXPECT_STREQ(fetched.pnr, booking.pnr);
    EXPECT_STREQ(fetched.passenger_id, passenger_session.user_id);
    EXPECT_EQ(fetched.status, RRS_BOOKING_CONFIRMED);
}

TEST_F(BookingTest, MultipleBookingsGenerateDistinctPNRs) {
    rrs_booking_t b1, b2, b3;
    ASSERT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_SLEEPER, 300.0, "key-1", &b1), RRS_OK);
    ASSERT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_SLEEPER, 300.0, "key-2", &b2), RRS_OK);
    ASSERT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_SLEEPER, 300.0, "key-3", &b3), RRS_OK);

    EXPECT_STRNE(b1.pnr, b2.pnr);
    EXPECT_STRNE(b2.pnr, b3.pnr);
    EXPECT_STRNE(b1.pnr, b3.pnr);

    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_SLEEPER), 3);
}

/* ========================================================================= */
/* FR-019: Reject Booking on Payment Failure                                 */
/* ========================================================================= */

TEST_F(BookingTest, PaymentFailureRejectsBookingAndCreatesNoPNR) {
    /* Simulate payment failure */
    booking_set_mock_payment_result(0);

    rrs_booking_t booking;
    std::memset(&booking, 0xFF, sizeof(booking));

    rrs_status_t st = booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 850.0,
                                     "failed-payment-key", &booking);
    EXPECT_EQ(st, ERR_PAYMENT_FAILED);

    /* Output buffer must be zeroed on error */
    EXPECT_EQ(booking.pnr[0], '\0');
    EXPECT_EQ(booking.payment_success, 0);

    /* No confirmed booking recorded */
    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 0);

    /* Verify no booking exists */
    rrs_booking_t query;
    EXPECT_EQ(booking_get(&passenger_session, "PNR100000001", &query), ERR_NOT_FOUND);
}

TEST_F(BookingTest, PaymentGatewayCallbackCanControlOutcome) {
    /* Custom gateway rejecting transactions over 5000 */
    booking_set_payment_gateway([](const char*, double amount) -> int {
        return (amount <= 5000.0) ? 1 : 0;
    });

    rrs_booking_t b_ok, b_fail;
    EXPECT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 450.0, "txn-ok", &b_ok), RRS_OK);
    EXPECT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 5500.0, "txn-bad", &b_fail), ERR_PAYMENT_FAILED);

    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 1);
}

/* ========================================================================= */
/* Input Validation & Authorization (FR-004, SEC-003, SEC-005)               */
/* ========================================================================= */

TEST_F(BookingTest, RejectsInvalidInputs) {
    rrs_booking_t b;

    /* Null session */
    EXPECT_EQ(booking_create(nullptr, "EXP101", RRS_CLASS_AC3, 100.0, nullptr, &b), ERR_INVALID_INPUT);

    /* Null train id */
    EXPECT_EQ(booking_create(&passenger_session, nullptr, RRS_CLASS_AC3, 100.0, nullptr, &b), ERR_INVALID_INPUT);

    /* Null out buffer */
    EXPECT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 100.0, nullptr, nullptr), ERR_INVALID_INPUT);

    /* Non-positive fare */
    EXPECT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 0.0, nullptr, &b), ERR_INVALID_INPUT);
    EXPECT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, -50.0, nullptr, &b), ERR_INVALID_INPUT);

    /* Invalid travel class */
    EXPECT_EQ(booking_create(&passenger_session, "EXP101", static_cast<rrs_class_t>(99), 100.0, nullptr, &b), ERR_INVALID_INPUT);
}

TEST_F(BookingTest, RejectsNonBookableOrRetiredTrain) {
    rrs_booking_t b;

    /* Unknown train */
    EXPECT_EQ(booking_create(&passenger_session, "UNKNOWN99", RRS_CLASS_AC3, 100.0, nullptr, &b), ERR_NOT_FOUND);

    /* Retire train EXP101 */
    ASSERT_EQ(train_retire(&admin_session, "EXP101"), RRS_OK);

    /* Retired train is not bookable */
    EXPECT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 100.0, nullptr, &b), ERR_NOT_FOUND);
}

TEST_F(BookingTest, RejectsAdminSessionCallingPassengerBooking) {
    rrs_booking_t b;
    /* Admin cannot create passenger bookings directly */
    EXPECT_EQ(booking_create(&admin_session, "EXP101", RRS_CLASS_AC3, 500.0, nullptr, &b), ERR_FORBIDDEN);
}

TEST_F(BookingTest, SEC005NoSensitivePaymentDataInBookingRecord) {
    rrs_booking_t b;
    ASSERT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_AC2, 1200.0, "pay-sec", &b), RRS_OK);

    /* SEC-005: struct only holds payment_success flag, no card/account number fields */
    EXPECT_EQ(b.payment_success, 1);
    EXPECT_DOUBLE_EQ(b.fare_amount, 1200.0);
}

/* ========================================================================= */
/* FR-024: Prevent Duplicate Booking on Retry (Idempotency Key)              */
/* ========================================================================= */

TEST_F(BookingTest, IdempotentRetryReturnsExistingBookingWithoutDuplicate) {
    rrs_booking_t first_booking;
    rrs_booking_t retry_booking;

    /* First attempt */
    rrs_status_t st1 = booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 850.0,
                                      "idempotency-key-777", &first_booking);
    EXPECT_EQ(st1, RRS_OK);
    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 1);

    /* Retry attempt with same idempotency key */
    rrs_status_t st2 = booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 850.0,
                                      "idempotency-key-777", &retry_booking);
    EXPECT_EQ(st2, RRS_OK);

    /* Returns same PNR and booking details */
    EXPECT_STREQ(retry_booking.pnr, first_booking.pnr);
    EXPECT_EQ(retry_booking.status, first_booking.status);

    /* Seat count must NOT increase */
    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 1);
}

/* ========================================================================= */
/* Waitlist & Auto-Promotion (FR-020, FR-021, FR-022)                       */
/* ========================================================================= */

TEST_F(BookingTest, EnqueuesWaitlistWhenClassFullAndAutoPromotesOnCancel) {
    /* Create train with tiny capacity: 1 seat in AC2 */
    rrs_train_t small_train = make_train("TINY1", 10, 10, 1);
    ASSERT_EQ(train_create(&admin_session, &small_train), RRS_OK);

    /* 1st passenger books the only AC2 seat -> CONFIRMED */
    rrs_booking_t b1;
    ASSERT_EQ(booking_create(&passenger_session, "TINY1", RRS_CLASS_AC2, 1000.0, "b1-key", &b1), RRS_OK);
    EXPECT_EQ(b1.status, RRS_BOOKING_CONFIRMED);
    EXPECT_EQ(b1.waitlist_position, 0);

    /* Create second passenger */
    ASSERT_EQ(auth_register("pass2@rrs.test", "Pass2Pass!", "Second Passenger"), RRS_OK);
    rrs_session_t session2;
    ASSERT_EQ(auth_login("pass2@rrs.test", "Pass2Pass!", &session2), RRS_OK);

    /* 2nd passenger books AC2 -> WAITLISTED (position 1) */
    rrs_booking_t b2;
    ASSERT_EQ(booking_create(&session2, "TINY1", RRS_CLASS_AC2, 1000.0, "b2-key", &b2), RRS_OK);
    EXPECT_EQ(b2.status, RRS_BOOKING_WAITLISTED);
    EXPECT_EQ(b2.waitlist_position, 1);
    EXPECT_EQ(booking_count_waitlisted("TINY1", RRS_CLASS_AC2), 1);

    /* Passenger 1 cancels ticket -> frees seat and auto-promotes Passenger 2 */
    ASSERT_EQ(booking_cancel(&passenger_session, b1.pnr), RRS_OK);

    /* Check passenger 1 status is now CANCELLED */
    rrs_booking_t check_b1;
    ASSERT_EQ(booking_get(&passenger_session, b1.pnr, &check_b1), RRS_OK);
    EXPECT_EQ(check_b1.status, RRS_BOOKING_CANCELLED);

    /* Check passenger 2 status is now auto-promoted to CONFIRMED! */
    rrs_booking_t check_b2;
    ASSERT_EQ(booking_get(&session2, b2.pnr, &check_b2), RRS_OK);
    EXPECT_EQ(check_b2.status, RRS_BOOKING_CONFIRMED);
    EXPECT_EQ(check_b2.waitlist_position, 0);
    EXPECT_EQ(booking_count_waitlisted("TINY1", RRS_CLASS_AC2), 0);
    EXPECT_EQ(booking_count_confirmed("TINY1", RRS_CLASS_AC2), 1);
}

TEST_F(BookingTest, RejectsIdempotencyKeyExceedingMaxLength) {
    std::string long_key(80, 'k');
    rrs_booking_t b;
    EXPECT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 850.0,
                             long_key.c_str(), &b),
              ERR_INVALID_INPUT);
    EXPECT_EQ(booking_count_confirmed("EXP101", RRS_CLASS_AC3), 0);
}

TEST_F(BookingTest, WaitlistPromoteDoesNotOverAllocateWhenCapacityFull) {
    /* 1-seat class */
    rrs_train_t t = make_train("T1", 1, 0, 0);
    ASSERT_EQ(train_create(&admin_session, &t), RRS_OK);

    ASSERT_EQ(auth_register("user1@rrs.test", "Pass1234!", "User One"), RRS_OK);
    rrs_session_t s1;
    ASSERT_EQ(auth_login("user1@rrs.test", "Pass1234!", &s1), RRS_OK);

    ASSERT_EQ(auth_register("user2@rrs.test", "Pass1234!", "User Two"), RRS_OK);
    rrs_session_t s2;
    ASSERT_EQ(auth_login("user2@rrs.test", "Pass1234!", &s2), RRS_OK);

    rrs_booking_t b1, b2;
    ASSERT_EQ(booking_create(&s1, "T1", RRS_CLASS_SLEEPER, 500.0, "k1", &b1), RRS_OK);
    EXPECT_EQ(b1.status, RRS_BOOKING_CONFIRMED);

    ASSERT_EQ(booking_create(&s2, "T1", RRS_CLASS_SLEEPER, 500.0, "k2", &b2), RRS_OK);
    EXPECT_EQ(b2.status, RRS_BOOKING_WAITLISTED);

    EXPECT_EQ(booking_count_confirmed("T1", RRS_CLASS_SLEEPER), 1);
    EXPECT_EQ(booking_count_waitlisted("T1", RRS_CLASS_SLEEPER), 1);

    /* Calling waitlist_promote directly when capacity is full must not over-allocate */
    EXPECT_EQ(waitlist_promote("T1", RRS_CLASS_SLEEPER), RRS_OK);

    EXPECT_EQ(booking_count_confirmed("T1", RRS_CLASS_SLEEPER), 1);
    EXPECT_EQ(booking_count_waitlisted("T1", RRS_CLASS_SLEEPER), 1);

    rrs_booking_t check_b2;
    ASSERT_EQ(booking_get(&s2, b2.pnr, &check_b2), RRS_OK);
    EXPECT_EQ(check_b2.status, RRS_BOOKING_WAITLISTED);
}

TEST_F(BookingTest, WaitlistEnqueuePreservesFcfsOrder) {
    /* Capacity for AC2 is 20 */
    rrs_train_t cap_train = make_train("CAP2", 10, 10, 2);
    ASSERT_EQ(train_create(&admin_session, &cap_train), RRS_OK);

    ASSERT_EQ(auth_register("p1@rrs.test", "Pass1Pass1", "P One"), RRS_OK);
    rrs_session_t s1;
    ASSERT_EQ(auth_login("p1@rrs.test", "Pass1Pass1", &s1), RRS_OK);

    ASSERT_EQ(auth_register("p2@rrs.test", "Pass2Pass2", "P Two"), RRS_OK);
    rrs_session_t s2;
    ASSERT_EQ(auth_login("p2@rrs.test", "Pass2Pass2", &s2), RRS_OK);

    ASSERT_EQ(auth_register("p3@rrs.test", "Pass3Pass3", "P Three"), RRS_OK);
    rrs_session_t s3;
    ASSERT_EQ(auth_login("p3@rrs.test", "Pass3Pass3", &s3), RRS_OK);

    ASSERT_EQ(auth_register("p4@rrs.test", "Pass4Pass4", "P Four"), RRS_OK);
    rrs_session_t s4;
    ASSERT_EQ(auth_login("p4@rrs.test", "Pass4Pass4", &s4), RRS_OK);

    rrs_booking_t b1, b2, b3, b4;
    ASSERT_EQ(booking_create(&s1, "CAP2", RRS_CLASS_AC2, 500.0, "k1", &b1), RRS_OK);
    ASSERT_EQ(booking_create(&s2, "CAP2", RRS_CLASS_AC2, 500.0, "k2", &b2), RRS_OK);
    ASSERT_EQ(booking_create(&s3, "CAP2", RRS_CLASS_AC2, 500.0, "k3", &b3), RRS_OK);
    ASSERT_EQ(booking_create(&s4, "CAP2", RRS_CLASS_AC2, 500.0, "k4", &b4), RRS_OK);

    /* b1, b2 are CONFIRMED */
    EXPECT_EQ(b1.status, RRS_BOOKING_CONFIRMED);
    EXPECT_EQ(b2.status, RRS_BOOKING_CONFIRMED);

    /* b3, b4 are WAITLISTED in FCFS order (FR-020) */
    EXPECT_EQ(b3.status, RRS_BOOKING_WAITLISTED);
    EXPECT_EQ(b3.waitlist_position, 1);

    EXPECT_EQ(b4.status, RRS_BOOKING_WAITLISTED);
    EXPECT_EQ(b4.waitlist_position, 2);

    EXPECT_EQ(booking_count_confirmed("CAP2", RRS_CLASS_AC2), 2);
    EXPECT_EQ(booking_count_waitlisted("CAP2", RRS_CLASS_AC2), 2);
}

TEST_F(BookingTest, CancelWaitlistedTicketRemovesFromQueueAndShiftsPositions) {
    rrs_train_t cap_train = make_train("CAP3", 10, 10, 1);
    ASSERT_EQ(train_create(&admin_session, &cap_train), RRS_OK);

    ASSERT_EQ(auth_register("w1@rrs.test", "Pass1Pass1", "W One"), RRS_OK);
    rrs_session_t s1;
    ASSERT_EQ(auth_login("w1@rrs.test", "Pass1Pass1", &s1), RRS_OK);

    ASSERT_EQ(auth_register("w2@rrs.test", "Pass2Pass2", "W Two"), RRS_OK);
    rrs_session_t s2;
    ASSERT_EQ(auth_login("w2@rrs.test", "Pass2Pass2", &s2), RRS_OK);

    ASSERT_EQ(auth_register("w3@rrs.test", "Pass3Pass3", "W Three"), RRS_OK);
    rrs_session_t s3;
    ASSERT_EQ(auth_login("w3@rrs.test", "Pass3Pass3", &s3), RRS_OK);

    rrs_booking_t b1, b2, b3;
    ASSERT_EQ(booking_create(&s1, "CAP3", RRS_CLASS_AC2, 500.0, "k1", &b1), RRS_OK); // Confirmed
    ASSERT_EQ(booking_create(&s2, "CAP3", RRS_CLASS_AC2, 500.0, "k2", &b2), RRS_OK); // Waitlist 1
    ASSERT_EQ(booking_create(&s3, "CAP3", RRS_CLASS_AC2, 500.0, "k3", &b3), RRS_OK); // Waitlist 2

    EXPECT_EQ(booking_count_waitlisted("CAP3", RRS_CLASS_AC2), 2);

    /* w2 (waitlist position 1) cancels ticket (FR-021) */
    ASSERT_EQ(booking_cancel(&s2, b2.pnr), RRS_OK);

    rrs_booking_t query_b2;
    ASSERT_EQ(booking_get(&s2, b2.pnr, &query_b2), RRS_OK);
    EXPECT_EQ(query_b2.status, RRS_BOOKING_CANCELLED);
    EXPECT_EQ(query_b2.waitlist_position, 0);

    /* w3 should now be shifted to waitlist position 1 */
    rrs_booking_t query_b3;
    ASSERT_EQ(booking_get(&s3, b3.pnr, &query_b3), RRS_OK);
    EXPECT_EQ(query_b3.status, RRS_BOOKING_WAITLISTED);
    EXPECT_EQ(query_b3.waitlist_position, 1);
    EXPECT_EQ(booking_count_waitlisted("CAP3", RRS_CLASS_AC2), 1);
}

TEST_F(BookingTest, CancelTicketPermissionsAndIdempotency) {
    rrs_booking_t b;
    ASSERT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 850.0, "perm-k", &b), RRS_OK);

    /* Other passenger cannot cancel */
    ASSERT_EQ(auth_register("other@rrs.test", "PassOther1", "Other"), RRS_OK);
    rrs_session_t other_s;
    ASSERT_EQ(auth_login("other@rrs.test", "PassOther1", &other_s), RRS_OK);
    EXPECT_EQ(booking_cancel(&other_s, b.pnr), ERR_FORBIDDEN);

    /* Non-existent PNR */
    EXPECT_EQ(booking_cancel(&passenger_session, "PNR999999999"), ERR_NOT_FOUND);

    /* Invalid input */
    EXPECT_EQ(booking_cancel(nullptr, b.pnr), ERR_INVALID_INPUT);
    EXPECT_EQ(booking_cancel(&passenger_session, nullptr), ERR_INVALID_INPUT);

    /* Successful cancellation by owner */
    EXPECT_EQ(booking_cancel(&passenger_session, b.pnr), RRS_OK);

    /* Subsequent cancellation is idempotent and succeeds */
    EXPECT_EQ(booking_cancel(&passenger_session, b.pnr), RRS_OK);
}

TEST_F(BookingTest, PassengerBookingListRetrieval) {
    rrs_booking_t b1, b2;
    ASSERT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_SLEEPER, 300.0, "list-1", &b1), RRS_OK);
    ASSERT_EQ(booking_create(&passenger_session, "EXP101", RRS_CLASS_AC3, 800.0, "list-2", &b2), RRS_OK);

    rrs_booking_t list[10];
    int count = 0;
    EXPECT_EQ(booking_list_by_passenger(&passenger_session, list, 10, &count), RRS_OK);
    EXPECT_EQ(count, 2);
}

TEST_F(BookingTest, CancelThenPromoteNeverExceedsCapacity) {
    /* Create train with capacity: 2 seats in Sleeper */
    rrs_train_t t = make_train("CAP2T", 2, 0, 0);
    ASSERT_EQ(train_create(&admin_session, &t), RRS_OK);

    ASSERT_EQ(auth_register("cp1@rrs.test", "Pass1234!", "CP One"), RRS_OK);
    rrs_session_t s1;
    ASSERT_EQ(auth_login("cp1@rrs.test", "Pass1234!", &s1), RRS_OK);

    ASSERT_EQ(auth_register("cp2@rrs.test", "Pass1234!", "CP Two"), RRS_OK);
    rrs_session_t s2;
    ASSERT_EQ(auth_login("cp2@rrs.test", "Pass1234!", &s2), RRS_OK);

    ASSERT_EQ(auth_register("cp3@rrs.test", "Pass3Pass3", "CP Three"), RRS_OK);
    rrs_session_t s3;
    ASSERT_EQ(auth_login("cp3@rrs.test", "Pass3Pass3", &s3), RRS_OK);

    ASSERT_EQ(auth_register("cp4@rrs.test", "Pass4Pass4", "CP Four"), RRS_OK);
    rrs_session_t s4;
    ASSERT_EQ(auth_login("cp4@rrs.test", "Pass4Pass4", &s4), RRS_OK);

    rrs_booking_t b1, b2, b3, b4;
    ASSERT_EQ(booking_create(&s1, "CAP2T", RRS_CLASS_SLEEPER, 400.0, "k1", &b1), RRS_OK);
    ASSERT_EQ(booking_create(&s2, "CAP2T", RRS_CLASS_SLEEPER, 400.0, "k2", &b2), RRS_OK);
    ASSERT_EQ(booking_create(&s3, "CAP2T", RRS_CLASS_SLEEPER, 400.0, "k3", &b3), RRS_OK);
    ASSERT_EQ(booking_create(&s4, "CAP2T", RRS_CLASS_SLEEPER, 400.0, "k4", &b4), RRS_OK);

    EXPECT_EQ(b1.status, RRS_BOOKING_CONFIRMED);
    EXPECT_EQ(b2.status, RRS_BOOKING_CONFIRMED);
    EXPECT_EQ(b3.status, RRS_BOOKING_WAITLISTED);
    EXPECT_EQ(b4.status, RRS_BOOKING_WAITLISTED);

    EXPECT_EQ(booking_count_confirmed("CAP2T", RRS_CLASS_SLEEPER), 2);
    EXPECT_EQ(booking_count_waitlisted("CAP2T", RRS_CLASS_SLEEPER), 2);

    /* Cancel 1 confirmed booking -> b1 cancelled, b3 auto-promoted to confirmed */
    ASSERT_EQ(booking_cancel(&s1, b1.pnr), RRS_OK);

    /* Confirmed count must be exactly 2 (capacity limit not exceeded) */
    EXPECT_EQ(booking_count_confirmed("CAP2T", RRS_CLASS_SLEEPER), 2);
    EXPECT_EQ(booking_count_waitlisted("CAP2T", RRS_CLASS_SLEEPER), 1);

    rrs_booking_t chk3, chk4;
    ASSERT_EQ(booking_get(&s3, b3.pnr, &chk3), RRS_OK);
    EXPECT_EQ(chk3.status, RRS_BOOKING_CONFIRMED);
    EXPECT_EQ(chk3.waitlist_position, 0);

    ASSERT_EQ(booking_get(&s4, b4.pnr, &chk4), RRS_OK);
    EXPECT_EQ(chk4.status, RRS_BOOKING_WAITLISTED);
    EXPECT_EQ(chk4.waitlist_position, 1);

    /* Calling waitlist_promote directly when capacity is full must not over-allocate */
    EXPECT_EQ(waitlist_promote("CAP2T", RRS_CLASS_SLEEPER), RRS_OK);
    EXPECT_EQ(booking_count_confirmed("CAP2T", RRS_CLASS_SLEEPER), 2);
    EXPECT_EQ(booking_count_waitlisted("CAP2T", RRS_CLASS_SLEEPER), 1);
}

/* ========================================================================= */
/* NFR-009: No double-booking under concurrency                              */
/* Exactly one confirmed booking when 50 concurrent requests target last seat*/
/* ========================================================================= */

TEST_F(BookingTest, NFR009NoDoubleBookingUnder50ConcurrentRequestsForLastSeat) {
    const int kConcurrentRequests = 50;
    /* Create train with exactly 1 seat in AC2 */
    rrs_train_t t = make_train("RACE101", 0, 0, 1);
    ASSERT_EQ(train_create(&admin_session, &t), RRS_OK);

    /* Register and log in 50 distinct passenger sessions */
    std::vector<rrs_session_t> sessions(kConcurrentRequests);
    for (int i = 0; i < kConcurrentRequests; ++i) {
        std::string email = "user" + std::to_string(i) + "@race.test";
        std::string name = "RaceUser" + std::to_string(i);
        ASSERT_EQ(auth_register(email.c_str(), "Pass1234!", name.c_str()), RRS_OK);
        ASSERT_EQ(auth_login(email.c_str(), "Pass1234!", &sessions[i]), RRS_OK);
    }

    std::vector<rrs_booking_t> results(kConcurrentRequests);
    std::vector<rrs_status_t> statuses(kConcurrentRequests);
    std::vector<std::thread> threads;
    threads.reserve(kConcurrentRequests);

    std::atomic<bool> release_gate{false};

    for (int i = 0; i < kConcurrentRequests; ++i) {
        threads.emplace_back([&, i]() {
            while (!release_gate.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            std::string key = "race-key-" + std::to_string(i);
            statuses[i] = booking_create(&sessions[i], "RACE101", RRS_CLASS_AC2, 950.0,
                                         key.c_str(), &results[i]);
        });
    }

    /* Release all 50 threads simultaneously to contend for the last seat */
    release_gate.store(true, std::memory_order_release);

    for (auto& th : threads) {
        th.join();
    }

    int confirmed_count = 0;
    int waitlisted_count = 0;
    std::set<std::string> pnrs;
    std::vector<int> waitlist_positions;

    for (int i = 0; i < kConcurrentRequests; ++i) {
        ASSERT_EQ(statuses[i], RRS_OK);
        pnrs.insert(results[i].pnr);

        if (results[i].status == RRS_BOOKING_CONFIRMED) {
            confirmed_count++;
            EXPECT_EQ(results[i].waitlist_position, 0);
        } else if (results[i].status == RRS_BOOKING_WAITLISTED) {
            waitlisted_count++;
            EXPECT_GT(results[i].waitlist_position, 0);
            waitlist_positions.push_back(results[i].waitlist_position);
        }
    }

    /* Exactly 1 confirmed booking, 49 waitlisted */
    EXPECT_EQ(confirmed_count, 1);
    EXPECT_EQ(waitlisted_count, 49);
    EXPECT_EQ(pnrs.size(), static_cast<size_t>(kConcurrentRequests));

    /* Inventory and waitlist counters agree */
    EXPECT_EQ(booking_count_confirmed("RACE101", RRS_CLASS_AC2), 1);
    EXPECT_EQ(booking_count_waitlisted("RACE101", RRS_CLASS_AC2), 49);

    /* Waitlist positions must cover 1 through 49 with no duplicates */
    std::sort(waitlist_positions.begin(), waitlist_positions.end());
    for (int i = 0; i < 49; ++i) {
        EXPECT_EQ(waitlist_positions[i], i + 1);
    }
}

TEST_F(BookingTest, NFR009SameSessionConcurrentBookingsNoDataRace) {
    const int kConcurrentRequests = 8;
    /* Create train with sufficient seats */
    rrs_train_t t = make_train("SAMESESS1", 50, 50, 50);
    ASSERT_EQ(train_create(&admin_session, &t), RRS_OK);

    /* Use the single shared passenger_session across all 8 concurrent threads */
    std::vector<rrs_booking_t> results(kConcurrentRequests);
    std::vector<rrs_status_t> statuses(kConcurrentRequests);
    std::vector<std::thread> threads;
    threads.reserve(kConcurrentRequests);

    std::atomic<bool> release_gate{false};

    for (int i = 0; i < kConcurrentRequests; ++i) {
        threads.emplace_back([&, i]() {
            while (!release_gate.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            std::string key = "same-sess-key-" + std::to_string(i);
            statuses[i] = booking_create(&passenger_session, "SAMESESS1", RRS_CLASS_SLEEPER, 350.0,
                                         key.c_str(), &results[i]);
        });
    }

    release_gate.store(true, std::memory_order_release);

    for (auto& th : threads) {
        th.join();
    }

    std::set<std::string> pnrs;
    for (int i = 0; i < kConcurrentRequests; ++i) {
        ASSERT_EQ(statuses[i], RRS_OK);
        EXPECT_EQ(results[i].status, RRS_BOOKING_CONFIRMED);
        pnrs.insert(results[i].pnr);
    }
    EXPECT_EQ(pnrs.size(), static_cast<size_t>(kConcurrentRequests));
    EXPECT_EQ(booking_count_confirmed("SAMESESS1", RRS_CLASS_SLEEPER), kConcurrentRequests);

    /* Also verify concurrent reads (booking_get) with the same session */
    std::vector<std::thread> read_threads;
    std::vector<rrs_booking_t> read_results(kConcurrentRequests);
    std::vector<rrs_status_t> read_statuses(kConcurrentRequests);
    read_threads.reserve(kConcurrentRequests);
    release_gate.store(false, std::memory_order_relaxed);

    for (int i = 0; i < kConcurrentRequests; ++i) {
        read_threads.emplace_back([&, i]() {
            while (!release_gate.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            read_statuses[i] = booking_get(&passenger_session, results[i].pnr, &read_results[i]);
        });
    }

    release_gate.store(true, std::memory_order_release);
    for (auto& th : read_threads) {
        th.join();
    }

    for (int i = 0; i < kConcurrentRequests; ++i) {
        EXPECT_EQ(read_statuses[i], RRS_OK);
        EXPECT_STREQ(read_results[i].pnr, results[i].pnr);
    }

    /* Also verify concurrent booking_list_by_passenger with the same session */
    std::vector<std::thread> list_threads;
    std::vector<rrs_status_t> list_statuses(kConcurrentRequests);
    std::vector<int> actual_counts(kConcurrentRequests, 0);
    list_threads.reserve(kConcurrentRequests);
    release_gate.store(false, std::memory_order_relaxed);

    for (int i = 0; i < kConcurrentRequests; ++i) {
        list_threads.emplace_back([&, i]() {
            while (!release_gate.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            rrs_booking_t user_bookings[16];
            list_statuses[i] = booking_list_by_passenger(&passenger_session, user_bookings,
                                                         16, &actual_counts[i]);
        });
    }

    release_gate.store(true, std::memory_order_release);
    for (auto& th : list_threads) {
        th.join();
    }

    for (int i = 0; i < kConcurrentRequests; ++i) {
        EXPECT_EQ(list_statuses[i], RRS_OK);
        EXPECT_GE(actual_counts[i], kConcurrentRequests);
    }
}

/* ========================================================================= */
/* NFR-002: Booking confirmation time                                        */
/* Booking confirmation completes within 1s under normal load                */
/* ========================================================================= */

TEST_F(BookingTest, NFR002BookingConfirmationTimeUnderNormalLoadMeets1sLimit) {
    const int kIterations = 100;
    /* Train with ample capacity for 100 bookings */
    rrs_train_t t = make_train("PERF101", 150, 100, 50);
    ASSERT_EQ(train_create(&admin_session, &t), RRS_OK);

    std::vector<double> latencies_ms;
    latencies_ms.reserve(kIterations);

    for (int i = 0; i < kIterations; ++i) {
        rrs_booking_t b;
        std::string key = "perf-key-" + std::to_string(i);

        auto start = std::chrono::steady_clock::now();
        rrs_status_t st = booking_create(&passenger_session, "PERF101", RRS_CLASS_SLEEPER,
                                         350.0, key.c_str(), &b);
        auto elapsed = std::chrono::steady_clock::now() - start;

        ASSERT_EQ(st, RRS_OK);
        EXPECT_EQ(b.status, RRS_BOOKING_CONFIRMED);

        double ms = std::chrono::duration<double, std::milli>(elapsed).count();
        latencies_ms.push_back(ms);
    }

    std::sort(latencies_ms.begin(), latencies_ms.end());
    double p95 = latencies_ms[static_cast<size_t>(0.95 * (latencies_ms.size() - 1))];
    double max_lat = latencies_ms.back();

    std::printf("  booking confirmation: p95 %.3f ms, max %.3f ms (limit 1000.0 ms)\n",
                p95, max_lat);

    /* NFR-002 requires completion within 1 second (1000.0 ms) */
    EXPECT_LT(p95, 1000.0);
    EXPECT_LT(max_lat, 1000.0);
}

} // namespace
