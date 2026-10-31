#ifndef RRS_INVENTORY_H
#define RRS_INVENTORY_H

/*
 * Public API of the Inventory & Schedule module (owner: Adhithya).
 * Admin-only management of trains, routes, halts and per-class capacities.
 * Signatures follow Design doc section 4.3 (train_create / update / retire).
 */

#include "common_types.h"
#include "search.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Travel classes (SRS Appendix A). */
typedef enum {
    RRS_CLASS_SLEEPER = 0,
    RRS_CLASS_AC3     = 1,     /* AC 3-Tier */
    RRS_CLASS_AC2     = 2,     /* AC 2-Tier */
    RRS_CLASS_COUNT   = 3
} rrs_class_t;

#define RRS_MAX_CLASS_CAPACITY 10000   /* seats in one class of one train */

/* FR-010: check that an ordered list of halts forms a connected path that the
 * route-search graph can use. Pure check: needs no session and changes nothing.
 * Rules (the same ones search_graph_add_train enforces):
 *   - 2..RRS_MAX_HALTS halts
 *   - each station code is 1-5 upper-case letters/digits and appears once
 *   - the first halt is at 0 km, departs before 24:00 and has arrive == depart
 *   - the last halt has arrive == depart
 *   - at every halt arrive <= depart, and it arrives no earlier than the
 *     previous halt departed
 *   - distance strictly increases from halt to halt
 *   - times and distances stay within RRS_MAX_HALT_MINUTES / RRS_MAX_DISTANCE_KM
 *   RRS_OK              route is usable
 *   ERR_INVALID_INPUT   null pointer or any rule above broken            */
rrs_status_t inventory_validate_route(const rrs_halt_t* halts, int halt_count);

typedef enum {
    RRS_TRAIN_ACTIVE  = 0,
    RRS_TRAIN_RETIRED = 1
} rrs_train_status_t;

/* A train as the admin defines it (SRS Appendix B.2). The scheduled departure
 * is the first halt's depart_min. */
typedef struct {
    char               train_id[RRS_TRAIN_ID_LEN];   /* 1-10 letters/digits    */
    int                halt_count;
    rrs_halt_t         halts[RRS_MAX_HALTS];         /* first halt_count used  */
    int                capacity[RRS_CLASS_COUNT];    /* seats per class, 0..RRS_MAX_CLASS_CAPACITY;
                                                        0 = class not offered; at least one > 0 */
    rrs_train_status_t status;                       /* ignored on input       */
} rrs_train_t;

/* The inventory module owns the route graph used by search: every ACTIVE train
 * is loaded into it, so callers must not call search_graph_add_train directly.
 * Trains are kept in memory (persistence is added by the store module). */

/* FR-007: create a train. Admin only.
 *   RRS_OK                  train stored as ACTIVE and added to the route graph
 *   ERR_SESSION_EXPIRED     null, forged or expired session
 *   ERR_FORBIDDEN           session is not an admin session
 *   ERR_INVALID_INPUT       null train, bad train id, invalid route (see
 *                           inventory_validate_route) or bad capacities
 *   ERR_DUPLICATE           the id is already used, even by a retired train
 * On any failure nothing is stored.                                         */
rrs_status_t train_create(const rrs_session_t* session, const rrs_train_t* train);

/* Read one train, retired ones included. Any live session may call it.
 *   RRS_OK                  `out` filled in
 *   ERR_SESSION_EXPIRED     null, forged or expired session
 *   ERR_INVALID_INPUT       null train_id or `out`
 *   ERR_NOT_FOUND           no train with this id
 * On any failure `out` is zeroed.                                           */
rrs_status_t train_get(const rrs_session_t* session, const char* train_id, rrs_train_t* out);

#ifdef __cplusplus
}
#endif

#endif /* RRS_INVENTORY_H */
