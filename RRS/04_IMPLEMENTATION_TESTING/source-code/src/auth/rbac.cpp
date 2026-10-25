/*
 * rbac.cpp - role-based access control (FR-004, SEC-002).
 * Decisions are made against the server-side session record, never against
 * the fields of the caller's copy of the session.
 */
#include "auth.h"
#include "session.h"

#include <cstring>
#include <string>

extern "C" rrs_status_t auth_authorize(const rrs_session_t* session, rrs_role_t required_role) {
    if (!session) return ERR_SESSION_EXPIRED;

    /* the token must be NUL-terminated inside its buffer before we read it as a string */
    if (!std::memchr(session->token, '\0', RRS_TOKEN_LEN)) return ERR_SESSION_EXPIRED;

    rrs::SessionInfo info;
    if (rrs::session_validate(session->token, info) != rrs::SessionStatus::Valid)
        return ERR_SESSION_EXPIRED;

    /* the user id in the caller's copy must match what the server issued */
    if (!std::memchr(session->user_id, '\0', RRS_USER_ID_LEN) ||
        info.user_id != session->user_id)
        return ERR_SESSION_EXPIRED;

    /* decide on the STORED role, not session->role */
    if (info.role != required_role) return ERR_FORBIDDEN;
    return RRS_OK;
}
