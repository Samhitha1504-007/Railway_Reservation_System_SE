/*
 * auth_internal.h - validation helpers and test hooks. NOT part of the public
 * API: other modules must include auth.h only.
 */
#ifndef RRS_AUTH_INTERNAL_H
#define RRS_AUTH_INTERNAL_H

#include <string>

#include "common_types.h"

namespace rrs {

/* Email: <=100 chars, no spaces, one '@', non-empty local part,
 * domain containing a '.' that is not first/last. */
bool is_valid_email(const std::string& email);

/* Password: 8-64 chars with at least one upper, one lower and one digit. */
bool is_valid_password(const std::string& password);

/* Name: 1-100 chars, no control characters, not only whitespace. */
bool is_valid_name(const std::string& name);

/* Lower-case an email so uniqueness is case-insensitive. */
std::string normalize_email(const std::string& email);

}  // namespace rrs

/* ---- test hooks ---- */
void auth_reset_for_tests(void);                                  /* clear all users */
bool auth_get_stored_hash_for_tests(const char* email, std::string& hash_out);
/* Create a user with any role (admins cannot self-register). */
rrs_status_t auth_create_user_for_tests(const char* email, const char* password,
                                        const char* name, rrs_role_t role);

#endif
