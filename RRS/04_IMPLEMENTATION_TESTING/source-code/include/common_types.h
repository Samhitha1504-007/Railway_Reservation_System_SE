#ifndef RRS_COMMON_TYPES_H
#define RRS_COMMON_TYPES_H

/*
 * Shared types used by every RRS module (C and C++).
 * Status codes follow Architecture & Design Specification section 4.3/4.4.
 * Changing anything here needs agreement from all four module owners.
 */

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One status enum returned by every API function.
 * Numeric values of the original two names are unchanged. */
typedef enum {
    RRS_OK = 0,
    RRS_STATUS_OK = RRS_OK,        /* legacy alias of RRS_OK                       */
    RRS_STATUS_ERROR = 1,          /* legacy generic error; prefer a specific code */
    ERR_INVALID_INPUT = 2,         /* failed field validation                      */
    ERR_AUTH_FAILED = 3,           /* wrong credentials                            */
    ERR_FORBIDDEN = 4,             /* role not permitted for this action           */
    ERR_SESSION_EXPIRED = 5,       /* token invalid or expired                     */
    ERR_NOT_FOUND = 6,             /* unknown record (e.g. PNR)                    */
    ERR_DUPLICATE = 7,             /* resource already exists (e.g. email)         */
    ERR_CAPACITY_CONFLICT = 8,     /* capacity below confirmed bookings            */
    ERR_PAYMENT_FAILED = 9,        /* mock payment declined                        */
    ERR_NO_ROUTE = 10,             /* no path between stations                     */
    ERR_IO = 11                    /* file/DB read or write failure                */
} rrs_status_t;

typedef enum {
    RRS_ROLE_PASSENGER = 0,
    RRS_ROLE_ADMIN = 1
} rrs_role_t;

typedef struct {
    unsigned int train_id;
    unsigned int station_id;
} rrs_route_ref_t;

#define RRS_USER_ID_LEN 37   /* 36 chars + NUL    */
#define RRS_TOKEN_LEN   65   /* 64 hex chars + NUL */

/* Issued by auth_login(); protected API calls take one of these. */
typedef struct {
    char       user_id[RRS_USER_ID_LEN];
    char       token[RRS_TOKEN_LEN];
    rrs_role_t role;
    time_t     expiry;       /* absolute time after which the session is invalid */
} rrs_session_t;

#ifdef __cplusplus
}
#endif

#endif /* RRS_COMMON_TYPES_H */
