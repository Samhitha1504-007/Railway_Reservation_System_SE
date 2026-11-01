#ifndef RRS_BOOKING_H
#define RRS_BOOKING_H

/*
 * Public API of the Booking & Queue Management module (owner: Saathvik).
 * Handles transactional booking, PNR generation, payment verification,
 * waitlist management, and ticket cancellations.
 * Signatures follow Design doc section 4.3 and SRS section 5.4.
 */

#include "common_types.h"
#include "inventory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RRS_PNR_LEN             13  /* 12 chars + NUL (SRS Appendix B.3) */
#define RRS_IDEMPOTENCY_KEY_LEN 65  /* 64 chars + NUL                    */
#define RRS_MAX_BOOKINGS        10000 /* SRS NFR-008: at least 10,000 bookings */

typedef enum {
    RRS_BOOKING_CONFIRMED  = 0,
    RRS_BOOKING_WAITLISTED = 1,
    RRS_BOOKING_CANCELLED  = 2
} rrs_booking_status_t;

/* A booking record (SRS Appendix B.3). */
typedef struct {
    char                 pnr[RRS_PNR_LEN];
    char                 passenger_id[RRS_USER_ID_LEN];
    char                 train_id[RRS_TRAIN_ID_LEN];
    rrs_class_t          travel_class;
    rrs_booking_status_t status;
    double               fare_amount;
    int                  waitlist_position; /* 0 if CONFIRMED, 1-based if WAITLISTED */
    char                 idempotency_key[RRS_IDEMPOTENCY_KEY_LEN];
    int                  payment_success;   /* SEC-005: only success/fail flag stored */
} rrs_booking_t;

/*
 * FR-018 / FR-019 / FR-024:
 * Create a new booking for the authenticated passenger.
 * Performs mock payment authorization before confirming seat and generating PNR.
 *
 * Parameters:
 *   session         - Active passenger session.
 *   train_id        - ID of the train to book (must be bookable per train_is_bookable).
 *   travel_class    - Travel class (Sleeper, AC3, AC2).
 *   fare_amount     - Fare charged for this booking (must be > 0).
 *   idempotency_key - Unique key for retry-safe booking (may be NULL or empty).
 *   out             - Output buffer populated with booking details on success.
 *
 * Returns:
 *   RRS_OK              - Booking created successfully (CONFIRMED or WAITLISTED).
 *   ERR_SESSION_EXPIRED - Session is null or expired.
 *   ERR_FORBIDDEN       - Session role is not Passenger.
 *   ERR_INVALID_INPUT   - Null pointers, bad train_id, invalid class, or non-positive fare.
 *   ERR_NOT_FOUND       - Train does not exist or is retired.
 *   ERR_PAYMENT_FAILED  - Mock payment authorization failed (no booking or PNR created).
 */
rrs_status_t booking_create(const rrs_session_t* session,
                            const char* train_id,
                            rrs_class_t travel_class,
                            double fare_amount,
                            const char* idempotency_key,
                            rrs_booking_t* out);

/*
 * FR-023: Retrieve an existing booking by PNR.
 * Any authenticated session (passenger who owns the ticket or admin) may view it.
 *
 * Returns:
 *   RRS_OK              - `out` filled in.
 *   ERR_SESSION_EXPIRED - Session is null or expired.
 *   ERR_INVALID_INPUT   - Null pnr or out buffer.
 *   ERR_NOT_FOUND       - No booking exists with the given PNR.
 *   ERR_FORBIDDEN       - Passenger session does not own this ticket.
 */
rrs_status_t booking_get(const rrs_session_t* session,
                         const char* pnr,
                         rrs_booking_t* out);

/*
 * FR-021: Cancel a booking by PNR.
 * Only the owner of the ticket or an admin can cancel it.
 * If a confirmed booking is cancelled, auto-promotes the head of the waitlist (FR-022).
 *
 * Returns:
 *   RRS_OK              - Ticket cancelled successfully.
 *   ERR_SESSION_EXPIRED - Session is null or expired.
 *   ERR_INVALID_INPUT   - Null pnr.
 *   ERR_NOT_FOUND       - No booking exists with the given PNR.
 *   ERR_FORBIDDEN       - Caller is not the passenger who booked the ticket.
 */
rrs_status_t booking_cancel(const rrs_session_t* session, const char* pnr);

/*
 * FR-022: Internal waitlist promotion.
 * Promotes the first waitlisted passenger for `(train_id, cls)` to CONFIRMED.
 * Called automatically on cancellation, or can be invoked directly by system.
 */
rrs_status_t waitlist_promote(const char* train_id, rrs_class_t cls);

/*
 * FR-011 integration hook: count confirmed bookings for a train and class.
 * Registered with inventory_set_booking_counter().
 */
int booking_count_confirmed(const char* train_id, rrs_class_t cls);

/*
 * Count active waitlisted bookings for a train and class.
 */
int booking_count_waitlisted(const char* train_id, rrs_class_t cls);

/*
 * Payment Gateway simulation hook.
 * Return 1 for success, 0 for failure.
 */
typedef int (*rrs_payment_gateway_fn)(const char* passenger_id, double amount);

/*
 * Configure payment gateway function.
 * Pass NULL to restore default mock payment logic.
 */
void booking_set_payment_gateway(rrs_payment_gateway_fn fn);

/*
 * Override mock payment outcome directly (1 = success, 0 = failure, -1 = use gateway).
 * Useful for unit and negative tests (FR-018, FR-019).
 */
void booking_set_mock_payment_result(int result);

/*
 * Initialize the booking module and register hooks with inventory.
 */
void booking_init(void);

/*
 * Clear all bookings and reset waitlist state (used in testing and teardown).
 */
void booking_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* RRS_BOOKING_H */
