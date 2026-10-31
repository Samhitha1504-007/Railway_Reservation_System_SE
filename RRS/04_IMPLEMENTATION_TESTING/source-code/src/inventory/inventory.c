/* Owner: Adhithya; train, route and capacity management. */

#include "inventory.h"
#include "inventory_internal.h"

#include "auth.h"

#include <string.h>

#define INVENTORY_MAX_TRAINS 1024   /* SRS NFR-008: at least 500 trains */

/* Sorted by train_id so lookups are a binary search. */
static rrs_train_t g_trains[INVENTORY_MAX_TRAINS];
static int         g_train_count = 0;

/* The code must be NUL-terminated inside its buffer before it is read. */
static int station_code_ok(const char* code)
{
    const void* nul = memchr(code, '\0', RRS_STATION_CODE_LEN);
    size_t len;
    size_t i;

    if (nul == NULL) return 0;
    len = (size_t)((const char*)nul - code);
    if (len == 0) return 0;
    for (i = 0; i < len; ++i) {
        char c = code[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return 0;
    }
    return 1;
}

rrs_status_t inventory_validate_route(const rrs_halt_t* halts, int halt_count)
{
    int i, j;

    if (halts == NULL || halt_count < 2 || halt_count > RRS_MAX_HALTS)
        return ERR_INVALID_INPUT;

    for (i = 0; i < halt_count; ++i) {
        const rrs_halt_t* h = &halts[i];

        if (!station_code_ok(h->station)) return ERR_INVALID_INPUT;
        for (j = 0; j < i; ++j)
            if (strcmp(halts[j].station, h->station) == 0) return ERR_INVALID_INPUT;

        if (h->arrive_min < 0 || h->depart_min < h->arrive_min) return ERR_INVALID_INPUT;
        if (h->depart_min > RRS_MAX_HALT_MINUTES || h->distance_km > RRS_MAX_DISTANCE_KM)
            return ERR_INVALID_INPUT;

        if (i == 0) {
            if (h->distance_km != 0 || h->depart_min >= 24 * 60) return ERR_INVALID_INPUT;
            if (h->arrive_min != h->depart_min) return ERR_INVALID_INPUT;
        } else {
            if (i == halt_count - 1 && h->depart_min != h->arrive_min) return ERR_INVALID_INPUT;
            if (h->arrive_min < halts[i - 1].depart_min) return ERR_INVALID_INPUT;
            if (h->distance_km <= halts[i - 1].distance_km) return ERR_INVALID_INPUT;
        }
    }
    return RRS_OK;
}

/* ---- helpers ---- */

/* Train id: 1-10 letters/digits, NUL-terminated inside its buffer. */
static int train_id_ok(const char* id)
{
    const void* nul = memchr(id, '\0', RRS_TRAIN_ID_LEN);
    size_t len, i;

    if (nul == NULL) return 0;
    len = (size_t)((const char*)nul - id);
    if (len == 0) return 0;
    for (i = 0; i < len; ++i) {
        char c = id[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
            return 0;
    }
    return 1;
}

static int capacities_ok(const int capacity[RRS_CLASS_COUNT])
{
    long total = 0;
    int c;

    for (c = 0; c < RRS_CLASS_COUNT; ++c) {
        if (capacity[c] < 0 || capacity[c] > RRS_MAX_CLASS_CAPACITY) return 0;
        total += capacity[c];
    }
    return total > 0;
}

/* Index of the train, or the negated insertion point minus one if absent. */
static int find_train(const char* id)
{
    int lo = 0, hi = g_train_count - 1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = strcmp(id, g_trains[mid].train_id);
        if (cmp == 0) return mid;
        if (cmp < 0) hi = mid - 1; else lo = mid + 1;
    }
    return -(lo + 1);
}

/* Any live session, admin or passenger. */
static rrs_status_t require_live_session(const rrs_session_t* session)
{
    rrs_status_t st = auth_authorize(session, RRS_ROLE_ADMIN);
    if (st == ERR_FORBIDDEN) st = auth_authorize(session, RRS_ROLE_PASSENGER);
    return st;
}

/* Reload the route graph from the stored trains. Only ACTIVE trains are
 * searchable. */
static rrs_status_t rebuild_graph(void)
{
    int i;

    search_graph_clear();
    for (i = 0; i < g_train_count; ++i) {
        rrs_status_t st;
        if (g_trains[i].status != RRS_TRAIN_ACTIVE) continue;
        st = search_graph_add_train(g_trains[i].train_id, g_trains[i].halts,
                                    g_trains[i].halt_count);
        if (st != RRS_OK) return st;
    }
    return RRS_OK;
}

/* Copy the caller's definition into a stored record (status is not copied). */
static void store_definition(rrs_train_t* slot, const rrs_train_t* train)
{
    memset(slot->halts, 0, sizeof slot->halts);
    memcpy(slot->train_id, train->train_id, RRS_TRAIN_ID_LEN);
    slot->halt_count = train->halt_count;
    memcpy(slot->halts, train->halts, (size_t)train->halt_count * sizeof train->halts[0]);
    memcpy(slot->capacity, train->capacity, sizeof slot->capacity);
}

/* ---- public API ---- */

rrs_status_t train_create(const rrs_session_t* session, const rrs_train_t* train)
{
    rrs_status_t st = auth_authorize(session, RRS_ROLE_ADMIN);
    int pos;
    rrs_train_t* slot;

    if (st != RRS_OK) return st;
    if (train == NULL || !train_id_ok(train->train_id) ||
        !capacities_ok(train->capacity))
        return ERR_INVALID_INPUT;
    st = inventory_validate_route(train->halts, train->halt_count);
    if (st != RRS_OK) return st;

    pos = find_train(train->train_id);
    if (pos >= 0) return ERR_DUPLICATE;
    if (g_train_count == INVENTORY_MAX_TRAINS) return RRS_STATUS_ERROR;

    st = search_graph_add_train(train->train_id, train->halts, train->halt_count);
    if (st != RRS_OK) return st;

    pos = -pos - 1;
    memmove(&g_trains[pos + 1], &g_trains[pos],
            (size_t)(g_train_count - pos) * sizeof g_trains[0]);
    slot = &g_trains[pos];
    memset(slot, 0, sizeof *slot);
    store_definition(slot, train);
    slot->status = RRS_TRAIN_ACTIVE;
    ++g_train_count;
    return RRS_OK;
}

rrs_status_t train_update(const rrs_session_t* session, const rrs_train_t* train)
{
    rrs_status_t st = auth_authorize(session, RRS_ROLE_ADMIN);
    rrs_train_t previous;
    int pos;

    if (st != RRS_OK) return st;
    if (train == NULL || !train_id_ok(train->train_id) ||
        !capacities_ok(train->capacity))
        return ERR_INVALID_INPUT;
    st = inventory_validate_route(train->halts, train->halt_count);
    if (st != RRS_OK) return st;

    pos = find_train(train->train_id);
    if (pos < 0) return ERR_NOT_FOUND;
    if (g_trains[pos].status != RRS_TRAIN_ACTIVE) return ERR_INVALID_INPUT;   /* retired */

    previous = g_trains[pos];
    store_definition(&g_trains[pos], train);
    st = rebuild_graph();
    if (st != RRS_OK) {                      /* not expected after validation; keep the old schedule */
        g_trains[pos] = previous;
        rebuild_graph();
        return st;
    }
    return RRS_OK;
}

rrs_status_t train_retire(const rrs_session_t* session, const char* train_id)
{
    rrs_status_t st = auth_authorize(session, RRS_ROLE_ADMIN);
    int pos;

    if (st != RRS_OK) return st;
    if (train_id == NULL) return ERR_INVALID_INPUT;

    pos = find_train(train_id);
    if (pos < 0) return ERR_NOT_FOUND;
    if (g_trains[pos].status == RRS_TRAIN_RETIRED) return RRS_OK;

    g_trains[pos].status = RRS_TRAIN_RETIRED;
    st = rebuild_graph();
    if (st != RRS_OK) {                      /* not expected; keep the train active */
        g_trains[pos].status = RRS_TRAIN_ACTIVE;
        rebuild_graph();
        return st;
    }
    return RRS_OK;
}

int train_is_bookable(const char* train_id)
{
    int pos;

    if (train_id == NULL) return 0;
    pos = find_train(train_id);
    return pos >= 0 && g_trains[pos].status == RRS_TRAIN_ACTIVE;
}

rrs_status_t train_get(const rrs_session_t* session, const char* train_id, rrs_train_t* out)
{
    rrs_status_t st;
    int pos;

    if (out != NULL) memset(out, 0, sizeof *out);
    st = require_live_session(session);
    if (st != RRS_OK) return st;
    if (train_id == NULL || out == NULL) return ERR_INVALID_INPUT;

    pos = find_train(train_id);
    if (pos < 0) return ERR_NOT_FOUND;
    *out = g_trains[pos];
    return RRS_OK;
}

/* ---- test hooks ---- */

void inventory_reset_for_tests(void)
{
    g_train_count = 0;
    search_graph_clear();
}

int inventory_train_count_for_tests(void)
{
    return g_train_count;
}
