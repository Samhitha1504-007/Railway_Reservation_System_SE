#ifndef RRS_AUTH_H
#define RRS_AUTH_H

/*
 * Public API of the User Access & Profile module (owner: Ridhima).
 * Signatures follow Design doc section 4.3.
 */

#include "common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FR-001 / FR-002: register a new passenger.
 *   RRS_OK               account created
 *   ERR_INVALID_INPUT    bad email, weak password, or bad name
 *   ERR_DUPLICATE        email already registered (case-insensitive)
 *   ERR_IO               hashing/storage failure                        */
rrs_status_t auth_register(const char* email, const char* password, const char* name);

/* FR-003: log in and receive a session tagged with the user's role.
 *   RRS_OK               `out` holds a new session
 *   ERR_AUTH_FAILED      unknown email OR wrong password (deliberately the
 *                        same result, so callers cannot probe which emails exist)
 *   ERR_INVALID_INPUT    a null pointer was passed
 *   ERR_IO               secure random generation failed
 * On any failure `out` is zeroed.                                           */
rrs_status_t auth_login(const char* email, const char* password, rrs_session_t* out);

#ifdef __cplusplus
}
#endif

#endif /* RRS_AUTH_H */
