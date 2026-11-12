#ifndef RRS_FACADE_H
#define RRS_FACADE_H

/*
 * Facade between the TUI and the modules. The TUI calls only these functions.
 * Every call checks the session first, fares always come from fare_calculate()
 * (never from user input), and security events go to the audit log.
 */

#include "booking.h"
#include "common_types.h"
#include "fare_eco.h"
#include "inventory.h"
#include "search.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start audit logging (the folder of audit_path is created if missing) and the
 * booking module. An unwritable log file is not fatal: events stay in memory. */
rrs_status_t facade_init(const char* audit_path);

/* Load a small demo network and an admin account (admin@rrs.in / Adm1nPassw0rd)
 * so the TUI can be shown without a database. Call once at startup. */
rrs_status_t facade_seed_demo(void);

/* Search + price + rank in one call (FR-012 to FR-016).
 *   ERR_SESSION_EXPIRED  no or expired session
 *   ERR_INVALID_INPUT    bad station, date or options
 *   ERR_NO_ROUTE         the stations are not connected                    */
rrs_status_t facade_search(const rrs_session_t* session, const char* from, const char* to,
                           const char* date, rrs_class_t cls, rrs_concession_t concession,
                           rrs_sort_key_t sort, const rrs_filter_t* filter,
                           rrs_option_list_t* out);

/* Book every leg of a search option for the logged-in passenger. The fare of
 * each leg is recomputed here from the route, so the caller cannot choose it.
 * `key` is a retry key; leg i uses "<key>-<i>", so a retry never duplicates.
 * `bookings` needs room for RRS_MAX_LEGS entries; *count is how many were made.
 * If a later leg fails, the legs already booked are cancelled.              */
rrs_status_t facade_book(const rrs_session_t* session, const rrs_route_t* route,
                         rrs_class_t cls, rrs_concession_t concession, const char* key,
                         rrs_booking_t* bookings, int* count);

rrs_status_t facade_my_bookings(const rrs_session_t* session, rrs_booking_t* out, int max,
                                int* count);
rrs_status_t facade_cancel(const rrs_session_t* session, const char* pnr);

#ifdef __cplusplus
}
#endif

#endif /* RRS_FACADE_H */
