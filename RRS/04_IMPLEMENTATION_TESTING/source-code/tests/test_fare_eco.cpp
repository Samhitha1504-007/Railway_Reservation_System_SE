#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "fare_eco.h"

namespace {

/* A route from legs of the given distances (km) and a total time. */
rrs_route_t route(std::vector<int> leg_km, int total_minutes = 600) {
    rrs_route_t r;
    std::memset(&r, 0, sizeof r);
    r.leg_count = static_cast<int>(leg_km.size());
    for (size_t i = 0; i < leg_km.size(); ++i) {
        r.legs[i].distance_km = leg_km[i];
        r.total_distance_km += leg_km[i];
        std::snprintf(r.legs[i].train_id, sizeof r.legs[i].train_id, "T%d", static_cast<int>(i + 1));
    }
    r.total_minutes = total_minutes;
    return r;
}

double fare(const rrs_route_t& r, rrs_class_t c, rrs_concession_t k = RRS_CONCESSION_NONE) {
    double f = -1;
    EXPECT_EQ(RRS_OK, fare_calculate(&r, c, k, &f));
    return f;
}

}  // namespace

/* ---------- FR-014: fare ---------- */

TEST(FareTest, DirectSleeperNoConcessionMatchesHandComputedFare) {   /* UT-01 */
    EXPECT_EQ(320.0, fare(route({500}), RRS_CLASS_SLEEPER));            /* 20 + 500 x 0.60 */
}

TEST(FareTest, SeniorConcessionDiscountsTheDistanceCharge) {          /* UT-02 */
    EXPECT_EQ(200.0, fare(route({500}), RRS_CLASS_SLEEPER, RRS_CONCESSION_SENIOR));  /* 20 + 300 x 0.6 */
}

TEST(FareTest, OtherConcessions) {
    EXPECT_EQ(245.0, fare(route({500}), RRS_CLASS_SLEEPER, RRS_CONCESSION_STUDENT));  /* 20 + 300 x 0.75 */
    EXPECT_EQ(170.0, fare(route({500}), RRS_CLASS_SLEEPER, RRS_CONCESSION_CHILD));    /* 20 + 300 x 0.5 */
}

TEST(FareTest, ClassChangesTheRate) {
    EXPECT_EQ(840.0, fare(route({500}), RRS_CLASS_AC3));    /* 40 + 500 x 1.60 */
    EXPECT_EQ(1250.0, fare(route({500}), RRS_CLASS_AC2));   /* 50 + 500 x 2.40 */
}

TEST(FareTest, EachLegOfAConnectionIsASeparateTicket) {
    EXPECT_EQ(340.0, fare(route({300, 200}), RRS_CLASS_SLEEPER));   /* 2 x 20 + 500 x 0.60 */
}

TEST(FareTest, RoundedToTheNearestRupee) {
    EXPECT_EQ(220.0, fare(route({333}), RRS_CLASS_SLEEPER));   /* 20 + 199.8 */
}

TEST(FareTest, LongerTripCostsMore) {
    EXPECT_LT(fare(route({100}), RRS_CLASS_AC3), fare(route({101}), RRS_CLASS_AC3));
}

TEST(FareTest, InvalidInputIsRejected) {
    rrs_route_t r = route({500});
    double f = 99;
    EXPECT_EQ(ERR_INVALID_INPUT, fare_calculate(nullptr, RRS_CLASS_SLEEPER, RRS_CONCESSION_NONE, &f));
    EXPECT_EQ(0.0, f);
    EXPECT_EQ(ERR_INVALID_INPUT, fare_calculate(&r, RRS_CLASS_SLEEPER, RRS_CONCESSION_NONE, nullptr));
    EXPECT_EQ(ERR_INVALID_INPUT, fare_calculate(&r, static_cast<rrs_class_t>(RRS_CLASS_COUNT), RRS_CONCESSION_NONE, &f));
    EXPECT_EQ(ERR_INVALID_INPUT, fare_calculate(&r, RRS_CLASS_SLEEPER, static_cast<rrs_concession_t>(-1), &f));
    rrs_route_t empty = route({});
    EXPECT_EQ(ERR_INVALID_INPUT, fare_calculate(&empty, RRS_CLASS_SLEEPER, RRS_CONCESSION_NONE, &f));
    rrs_route_t zero = route({0});
    EXPECT_EQ(ERR_INVALID_INPUT, fare_calculate(&zero, RRS_CLASS_SLEEPER, RRS_CONCESSION_NONE, &f));
}

/* ---------- FR-015: eco-score ---------- */

TEST(EcoTest, Co2AndScoreForADirectSleeperTrip) {
    rrs_eco_t e;
    rrs_route_t r = route({500});
    ASSERT_EQ(RRS_OK, eco_score(&r, RRS_CLASS_SLEEPER, &e));
    EXPECT_DOUBLE_EQ(4.0, e.co2_kg);        /* 500 x 8 g */
    EXPECT_DOUBLE_EQ(60.0, e.car_co2_kg);   /* 500 x 120 g */
    EXPECT_EQ(93, e.score);                 /* 100 x (1 - 4/60) */
}

TEST(EcoTest, AcClassesEmitMorePerPassenger) {
    rrs_eco_t sl, ac3, ac2;
    rrs_route_t r = route({500});
    ASSERT_EQ(RRS_OK, eco_score(&r, RRS_CLASS_SLEEPER, &sl));
    ASSERT_EQ(RRS_OK, eco_score(&r, RRS_CLASS_AC3, &ac3));
    ASSERT_EQ(RRS_OK, eco_score(&r, RRS_CLASS_AC2, &ac2));
    EXPECT_LT(sl.co2_kg, ac3.co2_kg);
    EXPECT_LT(ac3.co2_kg, ac2.co2_kg);
    EXPECT_EQ(88, ac3.score);   /* 100 x (1 - 7/60) */
    EXPECT_EQ(83, ac2.score);   /* 100 x (1 - 10/60) */
}

TEST(EcoTest, EachChangeOfTrainAddsAPenalty) {
    rrs_eco_t direct, two_legs;
    rrs_route_t d = route({500}), c = route({250, 250});
    ASSERT_EQ(RRS_OK, eco_score(&d, RRS_CLASS_SLEEPER, &direct));
    ASSERT_EQ(RRS_OK, eco_score(&c, RRS_CLASS_SLEEPER, &two_legs));
    EXPECT_DOUBLE_EQ(4.04, two_legs.co2_kg);   /* (500 + 5) x 8 g */
    EXPECT_GT(two_legs.co2_kg, direct.co2_kg);
}

TEST(EcoTest, InvalidInputIsRejected) {
    rrs_eco_t e;
    rrs_route_t r = route({500}), empty = route({});
    EXPECT_EQ(ERR_INVALID_INPUT, eco_score(nullptr, RRS_CLASS_SLEEPER, &e));
    EXPECT_EQ(ERR_INVALID_INPUT, eco_score(&r, RRS_CLASS_SLEEPER, nullptr));
    EXPECT_EQ(ERR_INVALID_INPUT, eco_score(&r, static_cast<rrs_class_t>(-1), &e));
    EXPECT_EQ(ERR_INVALID_INPUT, eco_score(&empty, RRS_CLASS_SLEEPER, &e));
}

/* ---------- FR-016: sort and filter ---------- */

class RankTest : public ::testing::Test {
protected:
    rrs_route_list_t list;
    rrs_option_list_t out;
    void SetUp() override {
        std::memset(&list, 0, sizeof list);
        list.count = 3;
        list.routes[0] = route({300}, 300);        /* A: fastest; Rs 200; 2.40 kg  */
        list.routes[1] = route({100, 150}, 600);   /* B: slowest; Rs 190; 2.04 kg  */
        list.routes[2] = route({280}, 500);        /* C: cheapest; Rs 188; 2.24 kg */
        std::snprintf(list.routes[0].legs[0].train_id, RRS_TRAIN_ID_LEN, "A");
        std::snprintf(list.routes[1].legs[0].train_id, RRS_TRAIN_ID_LEN, "B");
        std::snprintf(list.routes[2].legs[0].train_id, RRS_TRAIN_ID_LEN, "C");
    }
    std::string order() const {
        std::string s;
        for (int i = 0; i < out.count; ++i) s += out.options[i].route.legs[0].train_id;
        return s;
    }
    rrs_status_t rank(rrs_sort_key_t key, const rrs_filter_t* f = nullptr) {
        return search_rank(&list, RRS_CLASS_SLEEPER, RRS_CONCESSION_NONE, key, f, &out);
    }
};

TEST_F(RankTest, SortByTimeFareAndEco) {
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_TIME));
    EXPECT_EQ("ACB", order());
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_FARE));
    EXPECT_EQ("CBA", order());
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_ECO));
    EXPECT_EQ("BCA", order());
}

TEST_F(RankTest, EachOptionCarriesItsFareAndEcoScore) {
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_FARE));
    EXPECT_EQ(188.0, out.options[0].fare);
    EXPECT_EQ(190.0, out.options[1].fare);
    EXPECT_EQ(200.0, out.options[2].fare);
    EXPECT_DOUBLE_EQ(2.24, out.options[0].eco.co2_kg);
}

TEST_F(RankTest, ConcessionIsReflectedInRankedFares) {
    ASSERT_EQ(RRS_OK, search_rank(&list, RRS_CLASS_SLEEPER, RRS_CONCESSION_SENIOR, RRS_SORT_TIME,
                                  nullptr, &out));
    EXPECT_EQ(128.0, out.options[0].fare);   /* 20 + 300 x 0.6 x 0.6 */
}

TEST_F(RankTest, Filters) {
    rrs_filter_t f{};
    f.max_fare = 195;
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_TIME, &f));
    EXPECT_EQ("CB", order());

    f = rrs_filter_t{};
    f.max_minutes = 550;
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_TIME, &f));
    EXPECT_EQ("AC", order());

    f = rrs_filter_t{};
    f.direct_only = 1;
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_FARE, &f));
    EXPECT_EQ("CA", order());

    f = rrs_filter_t{};
    f.min_eco_score = 93;
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_TIME, &f));
    EXPECT_EQ("ACB", order());
}

TEST_F(RankTest, FilterThatMatchesNothingGivesAnEmptyResult) {
    rrs_filter_t f{};
    f.max_fare = 50;
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_TIME, &f));
    EXPECT_EQ(0, out.count);
}

TEST_F(RankTest, TiesKeepTheSearchOrder) {
    list.routes[2] = list.routes[0];
    std::snprintf(list.routes[2].legs[0].train_id, RRS_TRAIN_ID_LEN, "C");
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_FARE));
    EXPECT_EQ("BAC", order());   /* A and C cost the same; A came first */
}

TEST_F(RankTest, RankingDoesNotChangeTheInput) {
    rrs_route_list_t before = list;
    ASSERT_EQ(RRS_OK, rank(RRS_SORT_FARE));
    EXPECT_EQ(0, std::memcmp(&before, &list, sizeof list));
}

TEST_F(RankTest, InvalidInputIsRejected) {
    rrs_filter_t f{};
    f.max_fare = -1;
    EXPECT_EQ(ERR_INVALID_INPUT, rank(RRS_SORT_TIME, &f));
    EXPECT_EQ(ERR_INVALID_INPUT, rank(static_cast<rrs_sort_key_t>(7)));
    EXPECT_EQ(ERR_INVALID_INPUT, search_rank(nullptr, RRS_CLASS_SLEEPER, RRS_CONCESSION_NONE,
                                             RRS_SORT_TIME, nullptr, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_rank(&list, RRS_CLASS_SLEEPER, RRS_CONCESSION_NONE,
                                             RRS_SORT_TIME, nullptr, nullptr));
    list.count = RRS_MAX_ROUTES + 1;
    EXPECT_EQ(ERR_INVALID_INPUT, rank(RRS_SORT_TIME));
    EXPECT_EQ(0, out.count);
}

/* ---------- with a real search ---------- */

TEST(RankIntegrationTest, RanksTheResultOfARealSearch) {
    search_graph_clear();
    rrs_halt_t a[] = {{"SBC", 360, 360, 0}, {"NDLS", 1800, 1800, 2400}};
    rrs_halt_t b[] = {{"SBC", 1200, 1200, 0}, {"MAS", 1500, 1510, 360}, {"NDLS", 3210, 3210, 2365}};
    ASSERT_EQ(RRS_OK, search_graph_add_train("FAST", a, 2));
    ASSERT_EQ(RRS_OK, search_graph_add_train("SLOW", b, 3));
    rrs_route_list_t routes;
    ASSERT_EQ(RRS_OK, search_routes("SBC", "NDLS", "2026-11-02", nullptr, &routes));
    ASSERT_EQ(2, routes.count);

    rrs_option_list_t out;
    ASSERT_EQ(RRS_OK, search_rank(&routes, RRS_CLASS_AC3, RRS_CONCESSION_NONE, RRS_SORT_FARE,
                                  nullptr, &out));
    ASSERT_EQ(2, out.count);
    EXPECT_STREQ("SLOW", out.options[0].route.legs[0].train_id);   /* 2365 km is cheaper */
    EXPECT_EQ(3824.0, out.options[0].fare);                       /* 40 + 2365 x 1.60 */
    search_graph_clear();
}
