#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "search.h"

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

rrs_status_t add(const char* id, std::vector<rrs_halt_t> halts) {
    return search_graph_add_train(id, halts.data(), static_cast<int>(halts.size()));
}

int hm(int h, int m) { return h * 60 + m; }

const char* kDate = "2026-11-02";

}  // namespace

class SearchTest : public ::testing::Test {
protected:
    rrs_route_list_t out;
    void SetUp() override {
        search_graph_clear();
        std::memset(&out, 0xAB, sizeof out);
    }
};

/* ---------- graph loading ---------- */

TEST_F(SearchTest, AddValidTrainSucceeds) {
    EXPECT_EQ(RRS_OK, add("12627", {halt("SBC", hm(6, 0), hm(6, 0), 0),
                                    halt("NDLS", hm(20, 0), hm(20, 0), 2400)}));
}

TEST_F(SearchTest, DuplicateTrainIdIsRejected) {
    ASSERT_EQ(RRS_OK, add("T1", {halt("A", 0, 0, 0), halt("B", 60, 60, 50)}));
    EXPECT_EQ(ERR_DUPLICATE, add("T1", {halt("C", 0, 0, 0), halt("D", 60, 60, 50)}));
}

TEST_F(SearchTest, InvalidTrainsAreRejected) {
    EXPECT_EQ(ERR_INVALID_INPUT, search_graph_add_train(nullptr, nullptr, 0));
    EXPECT_EQ(ERR_INVALID_INPUT, add("T1", {halt("A", 0, 0, 0)}));                          /* one halt */
    EXPECT_EQ(ERR_INVALID_INPUT, add("", {halt("A", 0, 0, 0), halt("B", 60, 60, 50)}));      /* empty id */
    EXPECT_EQ(ERR_INVALID_INPUT, add("T 1", {halt("A", 0, 0, 0), halt("B", 60, 60, 50)}));   /* space */
    EXPECT_EQ(ERR_INVALID_INPUT, add("T12345678901", {halt("A", 0, 0, 0), halt("B", 60, 60, 50)}));
    EXPECT_EQ(ERR_INVALID_INPUT, add("T1", {halt("a", 0, 0, 0), halt("B", 60, 60, 50)}));    /* lower case */
    EXPECT_EQ(ERR_INVALID_INPUT, add("T1", {halt("A", 0, 0, 0), halt("A", 60, 60, 50)}));    /* repeat */
    EXPECT_EQ(ERR_INVALID_INPUT, add("T1", {halt("A", 0, 0, 0), halt("B", 60, 50, 50)}));    /* dep < arr */
    EXPECT_EQ(ERR_INVALID_INPUT, add("T1", {halt("A", 0, 90, 0), halt("B", 60, 60, 50)}));   /* backwards */
    EXPECT_EQ(ERR_INVALID_INPUT, add("T1", {halt("A", 0, 0, 0), halt("B", 60, 60, 0)}));     /* km same */
    EXPECT_EQ(ERR_INVALID_INPUT, add("T1", {halt("A", 0, 0, 5), halt("B", 60, 60, 50)}));    /* km != 0 */
    EXPECT_EQ(ERR_INVALID_INPUT, add("T1", {halt("A", 1440, 1440, 0), halt("B", 1500, 1500, 50)}));
}

TEST_F(SearchTest, StationCodeWithoutTerminatorIsRejected) {
    rrs_halt_t a = halt("A", 0, 0, 0);
    std::memset(a.station, 'A', RRS_STATION_CODE_LEN);   /* no NUL */
    rrs_halt_t halts[] = {a, halt("B", 60, 60, 50)};
    EXPECT_EQ(ERR_INVALID_INPUT, search_graph_add_train("T1", halts, 2));
}

TEST_F(SearchTest, RejectedTrainIsNotLoaded) {
    ASSERT_EQ(ERR_INVALID_INPUT, add("T1", {halt("A", 0, 0, 0), halt("B", 60, 60, 0)}));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", kDate, nullptr, &out));   /* A unknown */
}

/* ---------- input validation ---------- */

class SearchInputTest : public SearchTest {
protected:
    void SetUp() override {
        SearchTest::SetUp();
        ASSERT_EQ(RRS_OK, add("T1", {halt("A", hm(8, 0), hm(8, 0), 0),
                                     halt("B", hm(10, 0), hm(10, 0), 120)}));
    }
};

TEST_F(SearchInputTest, UnknownStationIsInvalidInput) {           /* UT-03 */
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("XXX", "B", kDate, nullptr, &out));
    EXPECT_EQ(0, out.count);
}

TEST_F(SearchInputTest, NullArgumentsAreRejected) {
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes(nullptr, "B", kDate, nullptr, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", nullptr, kDate, nullptr, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", nullptr, nullptr, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", kDate, nullptr, nullptr));
}

TEST_F(SearchInputTest, SameOriginAndDestinationIsRejected) {
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "A", kDate, nullptr, &out));
}

TEST_F(SearchInputTest, MalformedStationCodesAreRejected) {
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("", "B", kDate, nullptr, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("a", "B", kDate, nullptr, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("ABCDEF", "B", kDate, nullptr, &out));
}

TEST_F(SearchInputTest, Dates) {
    EXPECT_EQ(RRS_OK, search_routes("A", "B", "2028-02-29", nullptr, &out));       /* leap year */
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", "2027-02-29", nullptr, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", "2026-13-01", nullptr, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", "2026-04-31", nullptr, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", "02-11-2026", nullptr, &out));
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", "2026-1-2", nullptr, &out));
}

TEST_F(SearchInputTest, OutOfRangeOptionsAreRejected) {
    rrs_search_opts_t o = search_default_opts();
    o.max_legs = 0;
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", kDate, &o, &out));
    o = search_default_opts();
    o.max_legs = RRS_MAX_LEGS + 1;
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", kDate, &o, &out));
    o = search_default_opts();
    o.max_results = RRS_MAX_ROUTES + 1;
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", kDate, &o, &out));
    o = search_default_opts();
    o.min_transfer_minutes = -1;
    EXPECT_EQ(ERR_INVALID_INPUT, search_routes("A", "B", kDate, &o, &out));
}

/* ---------- FR-012: direct search ---------- */

TEST_F(SearchTest, DirectSearchReturnsAllMatchingTrainsRanked) {   /* IT-01 */
    ASSERT_EQ(RRS_OK, add("SLOW", {halt("A", hm(6, 0), hm(6, 0), 0), halt("B", hm(11, 0), hm(11, 0), 300)}));
    ASSERT_EQ(RRS_OK, add("FAST", {halt("A", hm(9, 0), hm(9, 0), 0), halt("B", hm(12, 0), hm(12, 0), 300)}));
    ASSERT_EQ(RRS_OK, add("MID", {halt("A", hm(7, 0), hm(7, 0), 0), halt("B", hm(11, 0), hm(11, 0), 300)}));

    ASSERT_EQ(RRS_OK, search_routes("A", "B", kDate, nullptr, &out));
    ASSERT_EQ(3, out.count);
    EXPECT_STREQ("FAST", out.routes[0].legs[0].train_id);   /* 3h */
    EXPECT_STREQ("MID", out.routes[1].legs[0].train_id);    /* 4h */
    EXPECT_STREQ("SLOW", out.routes[2].legs[0].train_id);   /* 5h */
    for (int i = 0; i < 3; ++i) EXPECT_EQ(1, out.routes[i].leg_count);
}

TEST_F(SearchTest, DirectLegUsesIntermediateHaltsAndDistance) {
    ASSERT_EQ(RRS_OK, add("T1", {halt("A", hm(6, 0), hm(6, 0), 0),
                                 halt("B", hm(8, 0), hm(8, 10), 150),
                                 halt("C", hm(10, 0), hm(10, 5), 300),
                                 halt("D", hm(12, 0), hm(12, 0), 420)}));
    ASSERT_EQ(RRS_OK, search_routes("B", "C", kDate, nullptr, &out));
    ASSERT_EQ(1, out.count);
    const rrs_route_t& r = out.routes[0];
    EXPECT_STREQ("B", r.legs[0].from);
    EXPECT_STREQ("C", r.legs[0].to);
    EXPECT_EQ(hm(8, 10), r.legs[0].depart_min);
    EXPECT_EQ(hm(10, 0), r.legs[0].arrive_min);
    EXPECT_EQ(110, r.total_minutes);
    EXPECT_EQ(150, r.total_distance_km);
}

TEST_F(SearchTest, TrainGoingTheOtherWayIsNotReturned) {
    ASSERT_EQ(RRS_OK, add("T1", {halt("B", hm(6, 0), hm(6, 0), 0), halt("A", hm(8, 0), hm(8, 0), 100)}));
    EXPECT_EQ(ERR_NO_ROUTE, search_routes("A", "B", kDate, nullptr, &out));
    EXPECT_EQ(0, out.count);
}

TEST_F(SearchTest, OvernightTrainIsReportedOnTheTravelDate) {
    /* starts 22:00, reaches B at 02:00 next day, C at 05:00 */
    ASSERT_EQ(RRS_OK, add("NIGHT", {halt("A", hm(22, 0), hm(22, 0), 0),
                                    halt("B", hm(26, 0), hm(26, 10), 200),
                                    halt("C", hm(29, 0), hm(29, 0), 400)}));
    ASSERT_EQ(RRS_OK, search_routes("B", "C", kDate, nullptr, &out));
    ASSERT_EQ(1, out.count);
    EXPECT_EQ(hm(2, 10), out.routes[0].legs[0].depart_min);   /* 02:10 on the travel date */
    EXPECT_EQ(hm(5, 0), out.routes[0].legs[0].arrive_min);
}

TEST_F(SearchTest, MaxResultsTruncatesToTheBest) {
    for (int i = 0; i < 5; ++i) {
        std::string id = "T" + std::to_string(i);
        ASSERT_EQ(RRS_OK, add(id.c_str(), {halt("A", hm(6, 0), hm(6, 0), 0),
                                           halt("B", hm(7 + i, 0), hm(7 + i, 0), 100)}));
    }
    rrs_search_opts_t o = search_default_opts();
    o.max_results = 2;
    ASSERT_EQ(RRS_OK, search_routes("A", "B", kDate, &o, &out));
    ASSERT_EQ(2, out.count);
    EXPECT_STREQ("T0", out.routes[0].legs[0].train_id);
    EXPECT_STREQ("T1", out.routes[1].legs[0].train_id);
}

TEST_F(SearchTest, DisconnectedStationsGiveNoRoute) {
    ASSERT_EQ(RRS_OK, add("T1", {halt("A", 0, 0, 0), halt("B", 60, 60, 50)}));
    ASSERT_EQ(RRS_OK, add("T2", {halt("C", 0, 0, 0), halt("D", 60, 60, 50)}));
    EXPECT_EQ(ERR_NO_ROUTE, search_routes("A", "D", kDate, nullptr, &out));
}

/* ---------- FR-013: connected (multi-leg) search ---------- */

class ConnectionTest : public SearchTest {
protected:
    void SetUp() override {
        SearchTest::SetUp();
        /* no direct A-C train; T1 A->B, T2 B->C */
        ASSERT_EQ(RRS_OK, add("T1", {halt("A", hm(8, 0), hm(8, 0), 0), halt("B", hm(10, 0), hm(10, 0), 150)}));
        ASSERT_EQ(RRS_OK, add("T2", {halt("B", hm(11, 0), hm(11, 0), 0), halt("C", hm(13, 0), hm(13, 0), 180)}));
    }
};

TEST_F(ConnectionTest, MultiLegRouteViaBIsReturnedWhenNoDirectTrain) {   /* IT-02 */
    ASSERT_EQ(RRS_OK, search_routes("A", "C", kDate, nullptr, &out));
    ASSERT_EQ(1, out.count);
    const rrs_route_t& r = out.routes[0];
    ASSERT_EQ(2, r.leg_count);
    EXPECT_STREQ("T1", r.legs[0].train_id);
    EXPECT_STREQ("B", r.legs[0].to);
    EXPECT_STREQ("T2", r.legs[1].train_id);
    EXPECT_STREQ("B", r.legs[1].from);
    EXPECT_STREQ("C", r.legs[1].to);
    EXPECT_EQ(hm(11, 0), r.legs[1].depart_min);
    EXPECT_EQ(5 * 60, r.total_minutes);
    EXPECT_EQ(330, r.total_distance_km);
}

TEST_F(ConnectionTest, MaxLegsOneMeansDirectOnly) {
    rrs_search_opts_t o = search_default_opts();
    o.max_legs = 1;
    EXPECT_EQ(ERR_NO_ROUTE, search_routes("A", "C", kDate, &o, &out));
}

TEST_F(ConnectionTest, TooShortTransferWaitsForTheNextDaysRun) {
    rrs_search_opts_t o = search_default_opts();
    o.min_transfer_minutes = 90;   /* T2 leaves 60 min after T1 arrives */
    ASSERT_EQ(RRS_OK, search_routes("A", "C", kDate, &o, &out));
    ASSERT_EQ(1, out.count);
    EXPECT_EQ(hm(11, 0) + 24 * 60, out.routes[0].legs[1].depart_min);
    EXPECT_EQ(hm(13, 0) + 24 * 60, out.routes[0].legs[1].arrive_min);
    EXPECT_EQ(29 * 60, out.routes[0].total_minutes);
}

TEST_F(ConnectionTest, ConnectionIsDroppedWhenADirectTrainIsFaster) {
    ASSERT_EQ(RRS_OK, add("DIR", {halt("A", hm(9, 0), hm(9, 0), 0), halt("C", hm(12, 0), hm(12, 0), 300)}));
    ASSERT_EQ(RRS_OK, search_routes("A", "C", kDate, nullptr, &out));
    ASSERT_EQ(1, out.count);
    EXPECT_STREQ("DIR", out.routes[0].legs[0].train_id);
}

TEST_F(ConnectionTest, FasterConnectionIsListedBeforeASlowDirectTrain) {
    ASSERT_EQ(RRS_OK, add("DIR", {halt("A", hm(6, 0), hm(6, 0), 0), halt("C", hm(14, 0), hm(14, 0), 300)}));
    ASSERT_EQ(RRS_OK, search_routes("A", "C", kDate, nullptr, &out));
    ASSERT_EQ(2, out.count);
    EXPECT_EQ(2, out.routes[0].leg_count);                     /* 5h connection */
    EXPECT_STREQ("DIR", out.routes[1].legs[0].train_id);       /* 8h direct */
}

TEST_F(ConnectionTest, ThreeLegJourneyNeedsMaxLegsThree) {
    ASSERT_EQ(RRS_OK, add("T3", {halt("C", hm(14, 0), hm(14, 0), 0), halt("D", hm(16, 0), hm(16, 0), 100)}));
    EXPECT_EQ(ERR_NO_ROUTE, search_routes("A", "D", kDate, nullptr, &out));   /* default 2 legs */

    rrs_search_opts_t o = search_default_opts();
    o.max_legs = 3;
    ASSERT_EQ(RRS_OK, search_routes("A", "D", kDate, &o, &out));
    ASSERT_EQ(1, out.count);
    ASSERT_EQ(3, out.routes[0].leg_count);
    EXPECT_STREQ("T3", out.routes[0].legs[2].train_id);
    EXPECT_EQ(8 * 60, out.routes[0].total_minutes);
}

TEST_F(SearchTest, SameTrainsConnectingAtTwoStationsAreListedOnce) {
    /* T1 and T2 both serve B and X, so A->C can change at either */
    ASSERT_EQ(RRS_OK, add("T1", {halt("A", hm(8, 0), hm(8, 0), 0),
                                 halt("B", hm(9, 0), hm(9, 0), 50),
                                 halt("X", hm(10, 0), hm(10, 0), 100)}));
    ASSERT_EQ(RRS_OK, add("T2", {halt("B", hm(9, 0), hm(9, 0), 0),
                                 halt("X", hm(10, 30), hm(10, 30), 50),
                                 halt("C", hm(12, 0), hm(12, 0), 150)}));
    ASSERT_EQ(RRS_OK, search_routes("A", "C", kDate, nullptr, &out));
    ASSERT_EQ(1, out.count);
    EXPECT_STREQ("X", out.routes[0].legs[0].to);   /* change at X: 30 min wait beats next-day at B */
}
