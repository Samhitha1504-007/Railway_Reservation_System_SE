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

/* FR-004: role-based access check. Call this at the start of every protected
 * function (the facade does it for you).
 *   RRS_OK                session is live and its role equals `required_role`
 *   ERR_SESSION_EXPIRED   null session, unknown/forged token, or expired
 *   ERR_FORBIDDEN         session is live but its role is not `required_role`
 * The role and user id are checked against the server-side record, never against
 * the fields in the caller's copy, so editing them does not escalate privileges.
 * Roles are matched exactly: an admin session does not satisfy a passenger check. */
rrs_status_t auth_authorize(const rrs_session_t* session, rrs_role_t required_role);

/* FR-006: end a session (logout). The token stops working immediately.
 *   RRS_OK                the session was live and has been removed
 *   ERR_SESSION_EXPIRED   null, unknown, tampered, already logged out, or idle-expired
 * Sessions also end by themselves after 15 minutes without a successful call
 * (see kSessionTtlSeconds in session.h). */
rrs_status_t auth_logout(const rrs_session_t* session);

#ifdef __cplusplus
}
#endif

#endif /* RRS_AUTH_H */
