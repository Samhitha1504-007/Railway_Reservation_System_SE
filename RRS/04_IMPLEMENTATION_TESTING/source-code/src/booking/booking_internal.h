#ifndef RRS_BOOKING_INTERNAL_H
#define RRS_BOOKING_INTERNAL_H

#include "booking.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal helpers shared between booking.c and waitlist.c.
 */

int booking_internal_pnr_ok(const char* pnr);
int booking_internal_train_id_ok(const char* train_id);

/* Waitlist management helpers implemented in waitlist.c */
int waitlist_internal_enqueue(const char* train_id, rrs_class_t cls, const char* pnr);
int waitlist_internal_get_position(const char* train_id, rrs_class_t cls, const char* pnr);
int waitlist_internal_remove(const char* train_id, rrs_class_t cls, const char* pnr);
const char* waitlist_internal_pop_head(const char* train_id, rrs_class_t cls);
int waitlist_internal_count(const char* train_id, rrs_class_t cls);
void waitlist_internal_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* RRS_BOOKING_INTERNAL_H */
