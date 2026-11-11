/* Owner: Saathvik; waitlist queue management. */

#include "booking.h"
#include "booking_internal.h"

#include <string.h>

#define MAX_WAITLIST_ENTRIES RRS_MAX_BOOKINGS   /* NFR-008: every booking could be waitlisted */

typedef struct {
    char        train_id[RRS_TRAIN_ID_LEN];
    rrs_class_t cls;
    char        pnr[RRS_PNR_LEN];
} waitlist_node_t;

static waitlist_node_t g_waitlist[MAX_WAITLIST_ENTRIES];
static int             g_waitlist_count = 0;

int waitlist_internal_enqueue(const char* train_id, rrs_class_t cls, const char* pnr)
{
    if (!train_id || !pnr || cls < 0 || cls >= RRS_CLASS_COUNT) return -1;
    if (g_waitlist_count >= MAX_WAITLIST_ENTRIES) return -1;

    memset(&g_waitlist[g_waitlist_count], 0, sizeof(waitlist_node_t));
    strncpy(g_waitlist[g_waitlist_count].train_id, train_id, RRS_TRAIN_ID_LEN - 1);
    g_waitlist[g_waitlist_count].cls = cls;
    strncpy(g_waitlist[g_waitlist_count].pnr, pnr, RRS_PNR_LEN - 1);
    g_waitlist_count++;

    return waitlist_internal_get_position(train_id, cls, pnr);
}

int waitlist_internal_get_position(const char* train_id, rrs_class_t cls, const char* pnr)
{
    int pos = 0;
    int i;

    if (!train_id || !pnr || cls < 0 || cls >= RRS_CLASS_COUNT) return 0;

    for (i = 0; i < g_waitlist_count; ++i) {
        if (g_waitlist[i].cls == cls && strcmp(g_waitlist[i].train_id, train_id) == 0) {
            pos++;
            if (strcmp(g_waitlist[i].pnr, pnr) == 0) {
                return pos;
            }
        }
    }
    return 0;
}

int waitlist_internal_remove(const char* train_id, rrs_class_t cls, const char* pnr)
{
    int i, j;

    if (!train_id || !pnr || cls < 0 || cls >= RRS_CLASS_COUNT) return 0;

    for (i = 0; i < g_waitlist_count; ++i) {
        if (g_waitlist[i].cls == cls &&
            strcmp(g_waitlist[i].train_id, train_id) == 0 &&
            strcmp(g_waitlist[i].pnr, pnr) == 0) {
            for (j = i; j < g_waitlist_count - 1; ++j) {
                g_waitlist[j] = g_waitlist[j + 1];
            }
            g_waitlist_count--;
            memset(&g_waitlist[g_waitlist_count], 0, sizeof(waitlist_node_t));
            return 1;
        }
    }
    return 0;
}

const char* waitlist_internal_pop_head(const char* train_id, rrs_class_t cls)
{
    static char popped_pnr[RRS_PNR_LEN];
    int i, j;

    if (!train_id || cls < 0 || cls >= RRS_CLASS_COUNT) return NULL;

    for (i = 0; i < g_waitlist_count; ++i) {
        if (g_waitlist[i].cls == cls && strcmp(g_waitlist[i].train_id, train_id) == 0) {
            memset(popped_pnr, 0, sizeof(popped_pnr));
            strncpy(popped_pnr, g_waitlist[i].pnr, RRS_PNR_LEN - 1);
            for (j = i; j < g_waitlist_count - 1; ++j) {
                g_waitlist[j] = g_waitlist[j + 1];
            }
            g_waitlist_count--;
            memset(&g_waitlist[g_waitlist_count], 0, sizeof(waitlist_node_t));
            return popped_pnr;
        }
    }
    return NULL;
}

int waitlist_internal_count(const char* train_id, rrs_class_t cls)
{
    int count = 0;
    int i;

    if (!train_id || cls < 0 || cls >= RRS_CLASS_COUNT) return 0;

    for (i = 0; i < g_waitlist_count; ++i) {
        if (g_waitlist[i].cls == cls && strcmp(g_waitlist[i].train_id, train_id) == 0) {
            count++;
        }
    }
    return count;
}

void waitlist_internal_clear(void)
{
    memset(g_waitlist, 0, sizeof(g_waitlist));
    g_waitlist_count = 0;
}
