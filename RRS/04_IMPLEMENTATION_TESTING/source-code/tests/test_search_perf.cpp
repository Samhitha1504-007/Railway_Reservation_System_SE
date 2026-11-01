/*
 * NFR-001 / FR-017 / ST-02: 95% of searches return within 2 s on a network of
 * 200 stations and 500 trains. NFR-008: the route graph holds 500 trains.
 * The network is random but seeded, so every run uses the same one.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "fare_eco.h"
#include "search.h"

namespace {

const int kStations = 200;
const int kTrains = 500;
const int kQueries = 100;
const double kLimitMs = 2000.0;

std::string station(int i) { return "S" + std::to_string(i); }

/* Loads kTrains trains of 10-30 halts over kStations stations. */
void load_network(std::mt19937& rng) {
    search_graph_clear();
    for (int t = 0; t < kTrains; ++t) {
        int n = 10 + static_cast<int>(rng() % 21);
        std::vector<int> order(kStations);
        for (int i = 0; i < kStations; ++i) order[i] = i;
        std::shuffle(order.begin(), order.end(), rng);

        std::vector<rrs_halt_t> halts(n);
        int time = static_cast<int>(rng() % 1440), km = 0;
        for (int i = 0; i < n; ++i) {
            std::memset(&halts[i], 0, sizeof halts[i]);
            std::snprintf(halts[i].station, sizeof halts[i].station, "%s", station(order[i]).c_str());
            halts[i].arrive_min = time;
            if (i > 0 && i < n - 1) time += 5;   /* dwell, none at the ends */
            halts[i].depart_min = time;
            halts[i].distance_km = km;
            time += 20 + static_cast<int>(rng() % 60);
            km += 10 + static_cast<int>(rng() % 40);
        }
        std::string id = "T" + std::to_string(t);
        ASSERT_EQ(RRS_OK, search_graph_add_train(id.c_str(), halts.data(), n)) << id;
    }
}

/* 95th-percentile time in ms over kQueries random searches. */
double p95_ms(std::mt19937& rng, const rrs_search_opts_t& opts, int* found) {
    std::vector<double> ms;
    *found = 0;
    for (int q = 0; q < kQueries; ++q) {
        int a = static_cast<int>(rng() % kStations), b = static_cast<int>(rng() % kStations);
        if (a == b) b = (b + 1) % kStations;
        rrs_route_list_t out;
        rrs_option_list_t ranked;
        auto t0 = std::chrono::steady_clock::now();
        rrs_status_t st = search_routes(station(a).c_str(), station(b).c_str(), "2026-11-02", &opts, &out);
        if (st == RRS_OK) {
            /* a search as the passenger sees it: priced, scored and sorted */
            EXPECT_EQ(RRS_OK, search_rank(&out, RRS_CLASS_AC3, RRS_CONCESSION_NONE, RRS_SORT_FARE,
                                          nullptr, &ranked));
            ++*found;
        } else {
            EXPECT_EQ(ERR_NO_ROUTE, st);
        }
        ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    }
    std::sort(ms.begin(), ms.end());
    return ms[static_cast<size_t>(0.95 * (ms.size() - 1))];
}

}  // namespace

TEST(SearchPerformanceTest, DefaultSearchMeetsNfr001) {
    std::mt19937 rng(2026);
    load_network(rng);
    int found = 0;
    double p95 = p95_ms(rng, search_default_opts(), &found);
    std::printf("  default search: p95 %.1f ms, %d/%d found a route\n", p95, found, kQueries);
    EXPECT_LE(p95, kLimitMs);
    EXPECT_GT(found, kQueries / 2);   /* the network is well connected */
}

TEST(SearchPerformanceTest, ThreeTrainSearchMeetsNfr001) {
    std::mt19937 rng(2027);
    load_network(rng);
    rrs_search_opts_t opts = search_default_opts();
    opts.max_legs = RRS_MAX_LEGS;
    int found = 0;
    double p95 = p95_ms(rng, opts, &found);
    std::printf("  3-train search: p95 %.1f ms, %d/%d found a route\n", p95, found, kQueries);
    EXPECT_LE(p95, kLimitMs);
}
