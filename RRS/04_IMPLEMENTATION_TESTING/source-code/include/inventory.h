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

#ifdef __cplusplus
}
#endif

#endif /* RRS_INVENTORY_H */
