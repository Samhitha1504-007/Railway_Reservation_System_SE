/*
 * rbac.cpp - role-based access control (FR-004, SEC-002).
 * Decisions are made against the server-side session record, never against
 * the fields of the caller's copy of the session.
 */
#include "auth.h"
#include "session.h"

extern "C" rrs_status_t auth_authorize(const rrs_session_t* session, rrs_role_t required_role) {
    rrs::SessionInfo info;
    /* null, forged, tampered or idle-expired sessions all end here */
    if (rrs::session_authenticate(session, info) != rrs::SessionStatus::Valid)
        return ERR_SESSION_EXPIRED;

    /* decide on the STORED role, not session->role */
    if (info.role != required_role) return ERR_FORBIDDEN;
    return RRS_OK;
}
