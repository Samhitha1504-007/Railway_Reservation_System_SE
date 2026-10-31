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

/* FR-005: a user's own profile. Role and password are not part of it. */
typedef struct {
    char       user_id[RRS_USER_ID_LEN];
    char       email[101];     /* stored lower-case */
    char       name[101];
    rrs_role_t role;
} rrs_profile_t;

/* View the profile of the logged-in user (always their own, taken from the
 * server-side session, never from an id the caller supplies).
 *   RRS_OK                `out` filled in
 *   ERR_SESSION_EXPIRED   null, forged, tampered or idle-expired session
 *   ERR_NOT_FOUND         the account no longer exists
 *   ERR_INVALID_INPUT     `out` is null
 * On any failure `out` is zeroed. Any live session may view its own profile. */
rrs_status_t auth_profile_get(const rrs_session_t* session, rrs_profile_t* out);

/* Update the logged-in user's own name and/or email. Pass NULL to leave a
 * field unchanged. All changes are validated first and applied together, or
 * not at all.
 *   RRS_OK                updated
 *   ERR_INVALID_INPUT     both NULL, or a bad name/email
 *   ERR_DUPLICATE         the new email belongs to another account
 *   ERR_SESSION_EXPIRED / ERR_NOT_FOUND   as for auth_profile_get          */
rrs_status_t auth_profile_update(const rrs_session_t* session, const char* new_name,
                                 const char* new_email);

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
