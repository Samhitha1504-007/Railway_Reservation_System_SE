/* Owner: Adhithya; train, route and capacity management. */

#include "inventory.h"

#include <string.h>

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
