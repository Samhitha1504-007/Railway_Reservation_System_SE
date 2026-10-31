/*
 * session.h - session store used by the auth module (internal).
 * Sessions live on the server side; the copy a caller holds is never trusted.
 */
#ifndef RRS_SESSION_H
#define RRS_SESSION_H

#include "common_types.h"

#include <cstddef>
#include <string>

namespace rrs {

/* Idle timeout (FR-006): a session ends after this many seconds without a
 * successful authenticated call. Every successful call restarts the timer.
 * The SRS says "a defined period" without a number, so this value must be
 * written into the SRS (FR-006). */
const int kSessionTtlSeconds = 15 * 60;

/* Create a session for a user and register it. Fills `out`.
 * Returns false only if secure random generation fails. */
bool session_create(const std::string& user_id, rrs_role_t role, rrs_session_t& out);

enum class SessionStatus { Valid, Invalid, Expired };

/* What the server remembers about a session (the caller's copy is never trusted). */
struct SessionInfo {
    std::string user_id;
    rrs_role_t  role;
    time_t      expiry;
};

/* Check a caller's session against the server-side record.
 *   Valid   : known token, user id matches, not idle for too long.
 *             Counts as activity: the idle timer restarts. `info` is filled in.
 *   Invalid : null, malformed, unknown or tampered (user id does not match)
 *   Expired : was valid but idle too long; the session is removed          */
SessionStatus session_authenticate(const rrs_session_t* session, SessionInfo& info);

/* Remove the caller's session (logout). Same checks as session_authenticate.
 * Returns true only if a live session was removed. */
bool session_destroy(const rrs_session_t* session);

/* Number of sessions currently stored (used by tests). */
size_t session_count();

/* Remove every session (used by tests). */
void session_clear();

/* Test hook: freeze "now" at `t` so expiry can be tested exactly. 0 = real clock. */
void session_set_fixed_time_for_tests(time_t t);

}  // namespace rrs

#endif
