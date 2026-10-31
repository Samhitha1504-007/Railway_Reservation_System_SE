/*
 * inventory_internal.h - test hooks. NOT part of the public API: other modules
 * must include inventory.h only.
 */
#ifndef RRS_INVENTORY_INTERNAL_H
#define RRS_INVENTORY_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Remove every train and empty the route graph. */
void inventory_reset_for_tests(void);

/* Number of trains stored, retired ones included. */
int inventory_train_count_for_tests(void);

#ifdef __cplusplus
}
#endif

#endif
