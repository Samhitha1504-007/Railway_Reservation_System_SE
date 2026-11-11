/*
 * rbac.cpp - role-based access control (FR-004, SEC-002) with audit logging (SEC-006).
 * Decisions are made against the server-side session record, never against
 * the fields of the caller's copy of the session.
 */
#include "auth.h"
#include "session.h"
#include "audit.h"

namespace {

/* `strict`: the caller needs exactly this role, so a wrong role is an
 * unauthorised attempt worth logging, and an admin call is an admin action.
 * (auth_authorize stays quiet about wrong roles because callers such as
 * "admin or passenger" read functions legitimately try one role, then the other.) */
rrs_status_t check(const rrs_session_t* session, rrs_role_t required_role,
                   const char* function, bool strict) {
    rrs::SessionInfo info;
    /* null, forged, tampered or idle-expired sessions are never legitimate */
    if (rrs::session_authenticate(session, info) != rrs::SessionStatus::Valid) {
        audit_event("SESSION_REJECTED", "unknown", function, ERR_SESSION_EXPIRED);
        return ERR_SESSION_EXPIRED;
    }

    /* decide on the STORED role, not session->role */
    if (info.role != required_role) {
        if (strict) audit_event("ACCESS_DENIED", info.user_id.c_str(), function, ERR_FORBIDDEN);
        return ERR_FORBIDDEN;
    }
    if (strict && required_role == RRS_ROLE_ADMIN)
        audit_event("ADMIN_ACTION", info.user_id.c_str(), function, RRS_OK);
    return RRS_OK;
}

}  // namespace

extern "C" rrs_status_t auth_authorize(const rrs_session_t* session, rrs_role_t required_role) {
    return check(session, required_role, "auth_authorize", false);
}

extern "C" rrs_status_t auth_authorize_for(const rrs_session_t* session, rrs_role_t required_role,
                                           const char* function_name) {
    return check(session, required_role, function_name ? function_name : "unknown", true);
}
