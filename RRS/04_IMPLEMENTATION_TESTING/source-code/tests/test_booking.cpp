#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "auth.h"
#include "auth_internal.h"
#include "booking.h"
#include "booking_internal.h"
#include "inventory.h"
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

} // namespace
