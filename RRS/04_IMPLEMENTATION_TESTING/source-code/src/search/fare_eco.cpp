/*
 * fare_eco.cpp - dynamic fare (FR-014), eco-score (FR-015) and sort/filter of
 * search results (FR-016).
 */
#include "fare_eco.h"

#include <algorithm>
#include <cmath>

namespace {

/* Indexed by rrs_class_t: Sleeper, AC 3-Tier, AC 2-Tier. */
const double kRatePerKm[RRS_CLASS_COUNT]       = {0.60, 1.60, 2.40};
const double kReservationCharge[RRS_CLASS_COUNT] = {20.0, 40.0, 50.0};
const double kCo2GramsPerKm[RRS_CLASS_COUNT]   = {8.0, 14.0, 20.0};

/* Indexed by rrs_concession_t. */
const double kDistanceDiscount[RRS_CONCESSION_COUNT] = {0.0, 0.40, 0.25, 0.50};

const double kCarCo2GramsPerKm = 120.0;
const double kTransferPenaltyKm = 5.0;

bool class_ok(rrs_class_t cls) { return cls >= 0 && cls < RRS_CLASS_COUNT; }

bool route_ok(const rrs_route_t* r) {
    if (!r || r->leg_count < 1 || r->leg_count > RRS_MAX_LEGS) return false;
    for (int i = 0; i < r->leg_count; ++i)
        if (r->legs[i].distance_km <= 0) return false;
    return true;
}

}  // namespace

extern "C" rrs_status_t fare_calculate(const rrs_route_t* route, rrs_class_t cls,
                                       rrs_concession_t concession, double* fare_out) {
    if (!fare_out) return ERR_INVALID_INPUT;
    *fare_out = 0.0;
    if (!route_ok(route) || !class_ok(cls)) return ERR_INVALID_INPUT;
    if (concession < 0 || concession >= RRS_CONCESSION_COUNT) return ERR_INVALID_INPUT;

    double total = 0.0;
    for (int i = 0; i < route->leg_count; ++i) {
        double distance_charge = route->legs[i].distance_km * kRatePerKm[cls];
        total += kReservationCharge[cls] + distance_charge * (1.0 - kDistanceDiscount[concession]);
    }
    *fare_out = std::round(total);
    return RRS_OK;
}

extern "C" rrs_status_t eco_score(const rrs_route_t* route, rrs_class_t cls, rrs_eco_t* out) {
    if (!out) return ERR_INVALID_INPUT;
    *out = rrs_eco_t{};
    if (!route_ok(route) || !class_ok(cls)) return ERR_INVALID_INPUT;

    double km = 0.0;
    for (int i = 0; i < route->leg_count; ++i) km += route->legs[i].distance_km;
    double train_km = km + kTransferPenaltyKm * (route->leg_count - 1);

    out->co2_kg = train_km * kCo2GramsPerKm[cls] / 1000.0;
    out->car_co2_kg = km * kCarCo2GramsPerKm / 1000.0;
    double s = 100.0 * (1.0 - out->co2_kg / out->car_co2_kg);
    out->score = static_cast<int>(std::lround(std::clamp(s, 0.0, 100.0)));
    return RRS_OK;
}

extern "C" rrs_status_t search_rank(const rrs_route_list_t* routes, rrs_class_t cls,
                                    rrs_concession_t concession, rrs_sort_key_t key,
                                    const rrs_filter_t* filter, rrs_option_list_t* out) {
    if (!out) return ERR_INVALID_INPUT;
    out->count = 0;
    if (!routes || routes->count < 0 || routes->count > RRS_MAX_ROUTES) return ERR_INVALID_INPUT;
    if (!class_ok(cls) || concession < 0 || concession >= RRS_CONCESSION_COUNT) return ERR_INVALID_INPUT;
    if (key < RRS_SORT_TIME || key > RRS_SORT_ECO) return ERR_INVALID_INPUT;
    rrs_filter_t f{};
    if (filter) f = *filter;
    if (f.max_fare < 0 || f.max_minutes < 0 || f.min_eco_score < 0) return ERR_INVALID_INPUT;

    rrs_route_option_t opts[RRS_MAX_ROUTES];
    int n = 0;
    for (int i = 0; i < routes->count; ++i) {
        rrs_route_option_t o;
        o.route = routes->routes[i];
        if (fare_calculate(&o.route, cls, concession, &o.fare) != RRS_OK) return ERR_INVALID_INPUT;
        if (eco_score(&o.route, cls, &o.eco) != RRS_OK) return ERR_INVALID_INPUT;

        if (f.max_fare > 0 && o.fare > f.max_fare) continue;
        if (f.max_minutes > 0 && o.route.total_minutes > f.max_minutes) continue;
        if (f.min_eco_score > 0 && o.eco.score < f.min_eco_score) continue;
        if (f.direct_only && o.route.leg_count != 1) continue;
        opts[n++] = o;
    }

    auto by = [key](const rrs_route_option_t& a, const rrs_route_option_t& b) {
        switch (key) {
            case RRS_SORT_FARE: return a.fare < b.fare;
            case RRS_SORT_ECO:  return a.eco.co2_kg < b.eco.co2_kg;
            default:            return a.route.total_minutes < b.route.total_minutes;
        }
    };
    std::stable_sort(opts, opts + n, by);

    std::copy(opts, opts + n, out->options);
    out->count = n;
    return RRS_OK;
}
