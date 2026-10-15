/*
 * auth.h - public API of the User Access & Profile module (owner: Ridhima Jain).
 * Signatures follow Design doc section 4.3. Do not change without agreement
 * from all module owners (frozen after Sprint 1 review).
 */
#ifndef RRS_AUTH_H
#define RRS_AUTH_H

#include "../common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FR-001 / FR-002: register a new passenger.
 *   RRS_OK               account created
 *   ERR_INVALID_INPUT    bad email, weak password, or bad name
 *   ERR_DUPLICATE        email already registered (case-insensitive)
 *   ERR_IO               hashing/storage failure                        */
rrs_status_t auth_register(const char* email, const char* password, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* RRS_AUTH_H */
