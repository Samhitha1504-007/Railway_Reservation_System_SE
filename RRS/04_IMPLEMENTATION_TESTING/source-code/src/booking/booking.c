/* Owner: Saathvik; transactional booking, PNR, payment verification. */

#include "booking.h"
#include "booking_internal.h"
#include "auth.h"
#include "inventory.h"

#include <stdio.h>
#include <string.h>

static rrs_booking_t g_bookings[RRS_MAX_BOOKINGS];
static int           g_booking_count = 0;
static unsigned int  g_pnr_seed = 100000000;

static int                    g_mock_payment_override = -1;
static rrs_payment_gateway_fn g_payment_gateway = NULL;

int booking_internal_train_id_ok(const char* id)
{
    const void* nul;
    size_t len, i;

    if (!id) return 0;
    nul = memchr(id, '\0', RRS_TRAIN_ID_LEN);
    if (!nul) return 0;
    len = (size_t)((const char*)nul - id);
    if (len == 0) return 0;
    for (i = 0; i < len; ++i) {
        char c = id[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
            return 0;
    }
    return 1;
}

int booking_internal_pnr_ok(const char* pnr)
{
    const void* nul;
    size_t len, i;

    if (!pnr) return 0;
    nul = memchr(pnr, '\0', RRS_PNR_LEN);
    if (!nul) return 0;
    len = (size_t)((const char*)nul - pnr);
    if (len != 12) return 0;
    for (i = 0; i < len; ++i) {
        char c = pnr[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return 0;
    }
    return 1;
}

static void generate_pnr(char* out_pnr)
{
    snprintf(out_pnr, RRS_PNR_LEN, "PNR%09u", ++g_pnr_seed);
}

static int execute_mock_payment(const char* passenger_id, double amount)
{
    if (g_mock_payment_override != -1) {
        return g_mock_payment_override;
    }
    if (g_payment_gateway) {
        return g_payment_gateway(passenger_id, amount);
    }
    return (amount > 0.0) ? 1 : 0;
}

rrs_status_t booking_create(const rrs_session_t* session,
                            const char* train_id,
                            rrs_class_t travel_class,
                            double fare_amount,
                            const char* idempotency_key,
                            rrs_booking_t* out)
{
    rrs_status_t auth_st;
    rrs_train_t train;
    int confirmed;
    int is_waitlist = 0;
    int wait_pos = 0;
    char pnr[RRS_PNR_LEN];
    int i;

    if (out) {
        memset(out, 0, sizeof(rrs_booking_t));
    }

    if (!session || !train_id || !out) {
        return ERR_INVALID_INPUT;
    }
    if (fare_amount <= 0.0) {
        return ERR_INVALID_INPUT;
    }
    if (travel_class < 0 || travel_class >= RRS_CLASS_COUNT) {
        return ERR_INVALID_INPUT;
    }
    if (!booking_internal_train_id_ok(train_id)) {
        return ERR_INVALID_INPUT;
    }

    /* Verify passenger role and valid session */
    auth_st = auth_authorize(session, RRS_ROLE_PASSENGER);
    if (auth_st != RRS_OK) {
        return auth_st;
    }

    /* Idempotency check (FR-024): avoid duplicate on retry */
    if (idempotency_key && idempotency_key[0] != '\0') {
        for (i = 0; i < g_booking_count; ++i) {
            if (strcmp(g_bookings[i].passenger_id, session->user_id) == 0 &&
                strcmp(g_bookings[i].idempotency_key, idempotency_key) == 0) {
                *out = g_bookings[i];
                return RRS_OK;
            }
        }
    }

    /* Check train existence and bookability */
    if (!train_is_bookable(train_id)) {
        return ERR_NOT_FOUND;
    }

    /* Retrieve train capacity details */
    memset(&train, 0, sizeof(train));
    if (train_get(session, train_id, &train) != RRS_OK) {
        return ERR_NOT_FOUND;
    }

    if (train.capacity[travel_class] <= 0) {
        return ERR_INVALID_INPUT;
    }

    /* Mock payment authorization (FR-018, FR-019) */
    if (!execute_mock_payment(session->user_id, fare_amount)) {
        return ERR_PAYMENT_FAILED;
    }

    /* Check capacity vs confirmed count */
    confirmed = booking_count_confirmed(train_id, travel_class);
    if (confirmed >= train.capacity[travel_class]) {
        is_waitlist = 1;
    }

    if (g_booking_count >= RRS_MAX_BOOKINGS) {
        return ERR_CAPACITY_CONFLICT;
    }

    generate_pnr(pnr);

    if (is_waitlist) {
        wait_pos = waitlist_internal_enqueue(train_id, travel_class, pnr);
        if (wait_pos <= 0) {
            return ERR_CAPACITY_CONFLICT;
        }
    }

    /* SEC-005: Store only success/fail flag, no card details */
    memset(&g_bookings[g_booking_count], 0, sizeof(rrs_booking_t));
    strncpy(g_bookings[g_booking_count].pnr, pnr, RRS_PNR_LEN - 1);
    strncpy(g_bookings[g_booking_count].passenger_id, session->user_id, RRS_USER_ID_LEN - 1);
    strncpy(g_bookings[g_booking_count].train_id, train_id, RRS_TRAIN_ID_LEN - 1);
    g_bookings[g_booking_count].travel_class = travel_class;
    g_bookings[g_booking_count].status = is_waitlist ? RRS_BOOKING_WAITLISTED : RRS_BOOKING_CONFIRMED;
    g_bookings[g_booking_count].fare_amount = fare_amount;
    g_bookings[g_booking_count].waitlist_position = wait_pos;
    g_bookings[g_booking_count].payment_success = 1;

    if (idempotency_key && idempotency_key[0] != '\0') {
        strncpy(g_bookings[g_booking_count].idempotency_key, idempotency_key, RRS_IDEMPOTENCY_KEY_LEN - 1);
    }

    *out = g_bookings[g_booking_count];
    g_booking_count++;

    return RRS_OK;
}

rrs_status_t booking_get(const rrs_session_t* session,
                         const char* pnr,
                         rrs_booking_t* out)
{
    rrs_status_t auth_pass, auth_admin;
    int i;

    if (out) {
        memset(out, 0, sizeof(rrs_booking_t));
    }

    if (!session || !pnr || !out) {
        return ERR_INVALID_INPUT;
    }

    auth_pass = auth_authorize(session, RRS_ROLE_PASSENGER);
    auth_admin = auth_authorize(session, RRS_ROLE_ADMIN);
    if (auth_pass != RRS_OK && auth_admin != RRS_OK) {
        return (auth_pass == ERR_FORBIDDEN || auth_admin == ERR_FORBIDDEN)
                   ? ERR_FORBIDDEN
                   : ERR_SESSION_EXPIRED;
    }

    for (i = 0; i < g_booking_count; ++i) {
        if (strcmp(g_bookings[i].pnr, pnr) == 0) {
            if (auth_admin != RRS_OK && strcmp(session->user_id, g_bookings[i].passenger_id) != 0) {
                return ERR_FORBIDDEN;
            }
            if (g_bookings[i].status == RRS_BOOKING_WAITLISTED) {
                g_bookings[i].waitlist_position =
                    waitlist_internal_get_position(g_bookings[i].train_id,
                                                   g_bookings[i].travel_class,
                                                   g_bookings[i].pnr);
            }
            *out = g_bookings[i];
            return RRS_OK;
        }
    }

    return ERR_NOT_FOUND;
}

rrs_status_t booking_cancel(const rrs_session_t* session, const char* pnr)
{
    rrs_status_t auth_pass, auth_admin;
    int i;

    if (!session || !pnr) {
        return ERR_INVALID_INPUT;
    }

    auth_pass = auth_authorize(session, RRS_ROLE_PASSENGER);
    auth_admin = auth_authorize(session, RRS_ROLE_ADMIN);
    if (auth_pass != RRS_OK && auth_admin != RRS_OK) {
        return (auth_pass == ERR_FORBIDDEN || auth_admin == ERR_FORBIDDEN)
                   ? ERR_FORBIDDEN
                   : ERR_SESSION_EXPIRED;
    }

    for (i = 0; i < g_booking_count; ++i) {
        if (strcmp(g_bookings[i].pnr, pnr) == 0) {
            if (auth_admin != RRS_OK && strcmp(session->user_id, g_bookings[i].passenger_id) != 0) {
                return ERR_FORBIDDEN;
            }

            if (g_bookings[i].status == RRS_BOOKING_CANCELLED) {
                return RRS_OK;
            }

            if (g_bookings[i].status == RRS_BOOKING_WAITLISTED) {
                waitlist_internal_remove(g_bookings[i].train_id,
                                         g_bookings[i].travel_class,
                                         g_bookings[i].pnr);
                g_bookings[i].status = RRS_BOOKING_CANCELLED;
                g_bookings[i].waitlist_position = 0;
            } else if (g_bookings[i].status == RRS_BOOKING_CONFIRMED) {
                g_bookings[i].status = RRS_BOOKING_CANCELLED;
                /* Auto-promote next waitlisted passenger (FR-022) */
                waitlist_promote(g_bookings[i].train_id, g_bookings[i].travel_class);
            }

            return RRS_OK;
        }
    }

    return ERR_NOT_FOUND;
}

rrs_status_t waitlist_promote(const char* train_id, rrs_class_t cls)
{
    const char* head_pnr;
    int i;

    if (!train_id || cls < 0 || cls >= RRS_CLASS_COUNT) {
        return ERR_INVALID_INPUT;
    }

    head_pnr = waitlist_internal_pop_head(train_id, cls);
    if (!head_pnr) {
        return RRS_OK;
    }

    for (i = 0; i < g_booking_count; ++i) {
        if (strcmp(g_bookings[i].pnr, head_pnr) == 0) {
            g_bookings[i].status = RRS_BOOKING_CONFIRMED;
            g_bookings[i].waitlist_position = 0;
            return RRS_OK;
        }
    }

    return ERR_NOT_FOUND;
}

int booking_count_confirmed(const char* train_id, rrs_class_t cls)
{
    int count = 0;
    int i;

    if (!train_id || cls < 0 || cls >= RRS_CLASS_COUNT) return 0;

    for (i = 0; i < g_booking_count; ++i) {
        if (g_bookings[i].status == RRS_BOOKING_CONFIRMED &&
            g_bookings[i].travel_class == cls &&
            strcmp(g_bookings[i].train_id, train_id) == 0) {
            count++;
        }
    }
    return count;
}

int booking_count_waitlisted(const char* train_id, rrs_class_t cls)
{
    return waitlist_internal_count(train_id, cls);
}

void booking_set_payment_gateway(rrs_payment_gateway_fn fn)
{
    g_payment_gateway = fn;
}

void booking_set_mock_payment_result(int result)
{
    g_mock_payment_override = result;
}

void booking_init(void)
{
    inventory_set_booking_counter(booking_count_confirmed);
}

void booking_clear(void)
{
    memset(g_bookings, 0, sizeof(g_bookings));
    g_booking_count = 0;
    g_pnr_seed = 100000000;
    g_mock_payment_override = -1;
    g_payment_gateway = NULL;
    waitlist_internal_clear();
}
