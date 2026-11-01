#ifndef RRS_FARE_ECO_H
#define RRS_FARE_ECO_H

/*
 * Fare, eco-score and result ranking for the Search, Fare & Eco-Optimization
 * module (owner: Samhitha). fare_calculate() and eco_score() follow Design doc
 * section 4.3; eco_score() also takes the travel class, because emissions per
 * passenger depend on it.
 */

#include "common_types.h"
#include "inventory.h"   /* rrs_class_t */
#include "search.h"      /* rrs_route_t, rrs_route_list_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- FR-014: dynamic fare ---- */

typedef enum {
    RRS_CONCESSION_NONE    = 0,
    RRS_CONCESSION_SENIOR  = 1,   /* 40% off the distance charge */
    RRS_CONCESSION_STUDENT = 2,   /* 25% off the distance charge */
    RRS_CONCESSION_CHILD   = 3,   /* 50% off the distance charge */
    RRS_CONCESSION_COUNT   = 4
} rrs_concession_t;

/* Fare in rupees for a route option. Each leg is a separate ticket:
 *   leg fare = reservation charge + distance_km * per-km rate (by class),
 *   with the concession applied to the distance charge only.
 * Rates per km: Sleeper 0.60, AC 3-Tier 1.60, AC 2-Tier 2.40.
 * Reservation charge per leg: Sleeper 20, AC 3-Tier 40, AC 2-Tier 50.
 * The result is rounded to the nearest rupee.
 *   RRS_OK              `*fare_out` set
 *   ERR_INVALID_INPUT   null pointer, bad class or concession, or a route with
 *                       no legs / a leg with no distance                   */
rrs_status_t fare_calculate(const rrs_route_t* route, rrs_class_t cls,
                            rrs_concession_t concession, double* fare_out);

/* ---- FR-015: eco-score ---- */

typedef struct {
    double co2_kg;       /* estimated CO2 per passenger for the whole journey */
    double car_co2_kg;   /* the same distance by car, for comparison          */
    int    score;        /* 0-100, higher is greener: 100 * (1 - co2 / car)   */
} rrs_eco_t;

/* Eco-score for a route option, from approximate published emission factors
 * (SRS 2.5): grams of CO2 per passenger-km of Sleeper 8, AC 3-Tier 14,
 * AC 2-Tier 20, against a car at 120. Each change of train adds the
 * equivalent of 5 km of travel.
 *   RRS_OK              `*out` filled in
 *   ERR_INVALID_INPUT   null pointer, bad class, or a route with no legs    */
rrs_status_t eco_score(const rrs_route_t* route, rrs_class_t cls, rrs_eco_t* out);

/* ---- FR-016: sort and filter ---- */

typedef enum {
    RRS_SORT_TIME = 0,   /* shortest total travel time first */
    RRS_SORT_FARE = 1,   /* cheapest first                   */
    RRS_SORT_ECO  = 2    /* lowest CO2 per passenger first   */
} rrs_sort_key_t;

/* A zero field means "no limit". */
typedef struct {
    double max_fare;        /* rupees              */
    int    max_minutes;     /* total travel time   */
    int    min_eco_score;   /* 0-100               */
    int    direct_only;     /* 1 = no changes      */
} rrs_filter_t;

typedef struct {
    rrs_route_t route;
    double      fare;
    rrs_eco_t   eco;
} rrs_route_option_t;

typedef struct {
    int                count;
    rrs_route_option_t options[RRS_MAX_ROUTES];
} rrs_option_list_t;

/* Price and score an existing search result, drop options outside `filter`
 * (may be NULL) and order the rest by `key`; ties keep the search order.
 * No new search is run (SRS 5.3.2). An empty result after filtering is RRS_OK
 * with out->count == 0.
 *   ERR_INVALID_INPUT   null pointer, bad class, concession or sort key, or
 *                       a negative filter value                             */
rrs_status_t search_rank(const rrs_route_list_t* routes, rrs_class_t cls,
                         rrs_concession_t concession, rrs_sort_key_t key,
                         const rrs_filter_t* filter, rrs_option_list_t* out);

#ifdef __cplusplus
}
#endif

#endif /* RRS_FARE_ECO_H */
