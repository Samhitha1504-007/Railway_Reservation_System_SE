/*
 * common_types.h - types shared by all RRS modules.
 * Source: Architecture & Design Specification, section 4.3 / 4.4.
 * Changing anything here needs agreement from all four module owners.
 */
#ifndef RRS_COMMON_TYPES_H
#define RRS_COMMON_TYPES_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Single status enum returned by every API function (Design doc 4.4). */
typedef enum {
    RRS_OK = 0,
    ERR_INVALID_INPUT,     /* failed field validation              */
    ERR_AUTH_FAILED,       /* wrong credentials                    */
    ERR_FORBIDDEN,         /* role not permitted for this action   */
    ERR_SESSION_EXPIRED,   /* token invalid or expired             */
    ERR_NOT_FOUND,         /* unknown record (e.g. PNR)            */
    ERR_DUPLICATE,         /* resource already exists (e.g. email) */
    ERR_CAPACITY_CONFLICT, /* capacity below confirmed bookings    */
    ERR_PAYMENT_FAILED,    /* mock payment declined                */
    ERR_NO_ROUTE,          /* no path between stations             */
    ERR_IO                 /* file/DB read or write failure        */
} rrs_status_t;

typedef enum {
    RRS_ROLE_PASSENGER = 0,
    RRS_ROLE_ADMIN = 1
} rrs_role_t;

#define RRS_USER_ID_LEN 37   /* 36 chars + NUL */
#define RRS_TOKEN_LEN   65   /* 64 hex chars + NUL */

/* Issued by auth_login(); every protected API call takes one of these. */
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
