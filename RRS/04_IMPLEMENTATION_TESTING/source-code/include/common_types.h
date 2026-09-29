#ifndef RRS_COMMON_TYPES_H
#define RRS_COMMON_TYPES_H

/* Shared types; module ownership is assigned as implementation begins. */
typedef enum {
    RRS_STATUS_OK = 0,
    RRS_STATUS_ERROR = 1
} rrs_status_t;

typedef enum {
    RRS_ROLE_PASSENGER = 0,
    RRS_ROLE_ADMIN = 1
} rrs_role_t;

typedef struct {
    unsigned int train_id;
    unsigned int station_id;
} rrs_route_ref_t;

#endif /* RRS_COMMON_TYPES_H */
