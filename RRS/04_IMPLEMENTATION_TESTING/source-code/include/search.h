#ifndef RRS_SEARCH_H
#define RRS_SEARCH_H

/*
 * Public API of the Search, Fare & Eco-Optimization module (owner: Samhitha).
 * search_routes() follows Design doc section 4.3. Fare and eco-score
 * (FR-014, FR-015) are added in Sprint 2.
 *
 * Times are minutes after 00:00 on the train's (or journey's) start day and may
 * exceed 1440 for overnight running. All trains are assumed to run daily.
 */

#include "common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RRS_STATION_CODE_LEN 6    /* up to 5 chars + NUL, e.g. "NDLS"  */
#define RRS_TRAIN_ID_LEN     11   /* up to 10 chars + NUL (SRS B.2)    */
#define RRS_MAX_HALTS        64   /* stations on one train's route     */
#define RRS_MAX_LEGS         3    /* trains in one journey             */
#define RRS_MAX_ROUTES       20   /* options returned by one search    */
#define RRS_MAX_HALT_MINUTES (7 * 24 * 60)   /* latest halt time: a week of running */
#define RRS_MAX_DISTANCE_KM  10000           /* longest train route                */

/* ---- network loading (called by the inventory module) ---- */

/* One stop on a train's route. */
typedef struct {
    char station[RRS_STATION_CODE_LEN];
    int  arrive_min;    /* must equal depart_min at the first halt */
    int  depart_min;    /* must equal arrive_min at the last halt  */
    int  distance_km;   /* from the train's first halt (0 there)   */
} rrs_halt_t;

/* Add a train to the route graph.
 *   RRS_OK              train added
 *   ERR_INVALID_INPUT   bad id or station code, fewer than 2 or more than
 *                       RRS_MAX_HALTS halts, a station repeated, times going
 *                       backwards or above RRS_MAX_HALT_MINUTES, distance not
 *                       increasing or above RRS_MAX_DISTANCE_KM
 *   ERR_DUPLICATE       a train with this id is already loaded            */
rrs_status_t search_graph_add_train(const char* train_id, const rrs_halt_t* halts, int halt_count);

/* Remove every train (used on reload and by tests). */
void search_graph_clear(void);

/* ---- route search ---- */

/* One train segment of a journey. Times are minutes after 00:00 on the
 * travel date, so a leg after an overnight wait has times above 1440. */
typedef struct {
    char train_id[RRS_TRAIN_ID_LEN];
    char from[RRS_STATION_CODE_LEN];
    char to[RRS_STATION_CODE_LEN];
    int  depart_min;
    int  arrive_min;
    int  distance_km;
} rrs_leg_t;

typedef struct {
    int       leg_count;          /* 1 = direct, 2+ = connected (multi-leg) */
    rrs_leg_t legs[RRS_MAX_LEGS];
    int       total_minutes;      /* first departure to final arrival       */
    int       total_distance_km;
} rrs_route_t;

typedef struct {
    int         count;
    rrs_route_t routes[RRS_MAX_ROUTES];
} rrs_route_list_t;

typedef struct {
    int max_legs;              /* 1..RRS_MAX_LEGS                         */
    int max_results;           /* 1..RRS_MAX_ROUTES                       */
    int min_transfer_minutes;  /* minimum wait when changing trains, >= 0 */
} rrs_search_opts_t;

/* Defaults: max_legs 2, max_results RRS_MAX_ROUTES, min_transfer_minutes 30. */
rrs_search_opts_t search_default_opts(void);

/* FR-012 / FR-013: find journeys from `src` to `dst` on `date` (YYYY-MM-DD).
 * Returns every direct train, plus connected journeys when there is no direct
 * train or when a connection has a shorter total travel time than the fastest
 * direct train. A journey never uses the same train twice.
 * Results are ranked by total travel time, then fewer changes, then earlier
 * departure. `opts` may be NULL for the defaults.
 *   RRS_OK              `out` holds 1..max_results routes
 *   ERR_INVALID_INPUT   null pointer, unknown station, src == dst, bad date
 *                       or out-of-range opts
 *   ERR_NO_ROUTE        both stations exist but no journey connects them
 * On any failure out->count is 0. Session/role checks are done by the facade. */
rrs_status_t search_routes(const char* src, const char* dst, const char* date,
                           const rrs_search_opts_t* opts, rrs_route_list_t* out);

#ifdef __cplusplus
}
#endif

#endif /* RRS_SEARCH_H */
