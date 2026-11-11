#ifndef RRS_AUDIT_H
#define RRS_AUDIT_H

/*
 * Audit logger (SEC-006, Design doc 4.4).
 * Records security-relevant events: failed logins, rejected sessions,
 * forbidden attempts and administrator actions.
 *
 * One line per event:
 *   2026-11-03T10:15:30Z event=LOGIN_FAILED user=a@b.com fn=auth_login status=ERR_AUTH_FAILED
 *
 * NEVER pass passwords, session tokens or payment details to this module.
 * All text fields are cleaned: anything outside [A-Za-z0-9@._:-] becomes '_'
 * and each field is cut to a fixed length, so a caller cannot inject extra
 * lines or fake fields into the log.
 */

#include "common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start appending events to the file at `path` (e.g. "data/audit.log").
 * NULL or "" keeps events in memory only.
 *   RRS_OK    ready
 *   ERR_IO    the file could not be opened; events are still kept in memory */
rrs_status_t audit_init(const char* path);

/* Stop writing to the file (events stay in memory). */
void audit_close(void);

/* Record one event. `user` may be NULL (logged as "-"). Never fails the caller's
 * operation: the return value is RRS_OK or ERR_IO (file write failed). */
rrs_status_t audit_event(const char* event, const char* user, const char* function,
                         rrs_status_t status);

/* "ERR_FORBIDDEN" etc. for a status code. */
const char* audit_status_name(rrs_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* RRS_AUDIT_H */
