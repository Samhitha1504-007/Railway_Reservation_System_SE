#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "inventory.h"

namespace {

rrs_halt_t halt(const char* station, int arrive, int depart, int km) {
    rrs_halt_t h;
    std::memset(&h, 0, sizeof h);
    std::strncpy(h.station, station, RRS_STATION_CODE_LEN - 1);
    h.arrive_min = arrive;
    h.depart_min = depart;
    h.distance_km = km;
    return h;
}

/* NDLS 08:00 -> CNB 12:00 -> LKO 14:00 */
std::vector<rrs_halt_t> good_route() {
    return {halt("NDLS", 480, 480, 0), halt("CNB", 720, 725, 440), halt("LKO", 840, 840, 510)};
}

rrs_status_t validate(const std::vector<rrs_halt_t>& v) {
    return inventory_validate_route(v.data(), static_cast<int>(v.size()));
}

}  // namespace

/* ---------- FR-010: route validation ---------- */

TEST(RouteValidationTest, ConnectedRouteIsAccepted) {
    EXPECT_EQ(validate(good_route()), RRS_OK);
}

TEST(RouteValidationTest, TwoHaltsIsTheSmallestRoute) {
    EXPECT_EQ(validate({halt("A", 0, 0, 0), halt("B", 60, 60, 50)}), RRS_OK);
}

TEST(RouteValidationTest, NullAndBadCountsAreRejected) {
    auto r = good_route();
    EXPECT_EQ(inventory_validate_route(nullptr, 3), ERR_INVALID_INPUT);
    EXPECT_EQ(inventory_validate_route(r.data(), 0), ERR_INVALID_INPUT);
    EXPECT_EQ(inventory_validate_route(r.data(), 1), ERR_INVALID_INPUT);
    EXPECT_EQ(inventory_validate_route(r.data(), -3), ERR_INVALID_INPUT);
    EXPECT_EQ(inventory_validate_route(r.data(), RRS_MAX_HALTS + 1), ERR_INVALID_INPUT);
}

TEST(RouteValidationTest, MaxHaltsIsAccepted) {
    std::vector<rrs_halt_t> v;
    char code[RRS_STATION_CODE_LEN];
    for (int i = 0; i < RRS_MAX_HALTS; ++i) {
        std::snprintf(code, sizeof code, "S%d", i);
        v.push_back(halt(code, i * 10, i * 10, i * 5));
    }
    EXPECT_EQ(validate(v), RRS_OK);
}

TEST(RouteValidationTest, BadStationCodesAreRejected) {
    for (const char* bad : {"", "ndls", "ND-S", "ND S", "TOOLONG"}) {
        auto r = good_route();
        if (std::strlen(bad) >= RRS_STATION_CODE_LEN) {
            std::memcpy(r[1].station, bad, RRS_STATION_CODE_LEN);   /* no terminator */
        } else {
            r[1] = halt(bad, 720, 725, 440);
        }
        EXPECT_EQ(validate(r), ERR_INVALID_INPUT) << "code: '" << bad << "'";
    }
}

TEST(RouteValidationTest, RepeatedStationIsRejected) {
    auto r = good_route();
    r[2] = halt("NDLS", 840, 840, 510);
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);
}

TEST(RouteValidationTest, FirstHaltMustStartAtZeroKmWithoutDwell) {
    auto r = good_route();
    r[0].distance_km = 5;
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);

    r = good_route();
    r[0].arrive_min = 470;
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);

    r = good_route();
    r[0] = halt("NDLS", 24 * 60, 24 * 60, 0);
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);
}

TEST(RouteValidationTest, LastHaltMustNotDwell) {
    auto r = good_route();
    r[2].depart_min = 850;
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);
}

TEST(RouteValidationTest, TimesMustNotGoBackwards) {
    auto r = good_route();
    r[1].arrive_min = 400;                  /* before NDLS departed */
    r[1].depart_min = 725;
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);

    r = good_route();
    r[1].depart_min = 700;                  /* departs before it arrives */
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);

    r = good_route();
    r[2].arrive_min = r[2].depart_min = 700;   /* earlier than CNB departure */
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);

    r = good_route();
    r[1].arrive_min = -1;
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);
}

TEST(RouteValidationTest, DistanceMustIncrease) {
    auto r = good_route();
    r[1].distance_km = 0;
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);

    r = good_route();
    r[2].distance_km = 440;                 /* same as previous halt */
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);

    r = good_route();
    r[2].distance_km = 300;                 /* shorter than previous halt */
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);
}

TEST(RouteValidationTest, OvernightRunningIsAccepted) {
    EXPECT_EQ(validate({halt("A", 1380, 1380, 0), halt("B", 1500, 1500, 200)}), RRS_OK);
}

TEST(RouteValidationTest, TimesAndDistancesHaveUpperBounds) {
    auto r = good_route();
    r[2].arrive_min = r[2].depart_min = RRS_MAX_HALT_MINUTES;
    EXPECT_EQ(validate(r), RRS_OK);
    r[2].arrive_min = r[2].depart_min = RRS_MAX_HALT_MINUTES + 1;
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);

    r = good_route();
    r[2].distance_km = RRS_MAX_DISTANCE_KM;
    EXPECT_EQ(validate(r), RRS_OK);
    r[2].distance_km = RRS_MAX_DISTANCE_KM + 1;
    EXPECT_EQ(validate(r), ERR_INVALID_INPUT);
}
