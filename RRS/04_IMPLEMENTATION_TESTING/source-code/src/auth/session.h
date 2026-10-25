/*
 * session.h - session store used by the auth module (internal).
 * login creates sessions here; validation and logout (FR-006) are added later.
 */
#ifndef RRS_SESSION_H
#define RRS_SESSION_H

#include "common_types.h"

#include <cstddef>
#include <string>

namespace rrs {

/* How long a session lives after login. The SRS says "a defined period"
 * (FR-006) without a number, so this value must be written into the SRS. */
const int kSessionTtlSeconds = 15 * 60;

/* Create a session for a user and register it. Fills `out`.
 * Returns false only if secure random generation fails. */
bool session_create(const std::string& user_id, rrs_role_t role, rrs_session_t& out);

/* Number of sessions currently stored (used by tests). */
size_t session_count();

/* Remove every session (used by tests). */
void session_clear();

}  // namespace rrs

#endif
