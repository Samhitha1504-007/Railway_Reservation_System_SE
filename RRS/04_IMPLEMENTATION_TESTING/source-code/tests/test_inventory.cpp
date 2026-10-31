#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "auth.h"
#include "auth_internal.h"
#include "inventory.h"
#include "inventory_internal.h"
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

/* NDLS 08:00 -> CNB 12:00 -> LKO 14:00 */
std::vector<rrs_halt_t> good_route() {
    return {halt("NDLS", 480, 480, 0), halt("CNB", 720, 725, 440), halt("LKO", 840, 840, 510)};
}

rrs_status_t validate(const std::vector<rrs_halt_t>& v) {
    return inventory_validate_route(v.data(), static_cast<int>(v.size()));
}

rrs_train_t make_train(const char* id, const std::vector<rrs_halt_t>& halts,
                       int sleeper = 100, int ac3 = 50, int ac2 = 20) {
    rrs_train_t t;
    std::memset(&t, 0, sizeof t);
    std::strncpy(t.train_id, id, RRS_TRAIN_ID_LEN - 1);
    t.halt_count = static_cast<int>(halts.size());
    std::memcpy(t.halts, halts.data(), halts.size() * sizeof(rrs_halt_t));
    t.capacity[RRS_CLASS_SLEEPER] = sleeper;
    t.capacity[RRS_CLASS_AC3] = ac3;
    t.capacity[RRS_CLASS_AC2] = ac2;
    return t;
}

const char* kTravelDate = "2030-01-15";

bool searchable(const char* src, const char* dst) {
    rrs_route_list_t out;
    return search_routes(src, dst, kTravelDate, nullptr, &out) == RRS_OK && out.count > 0;
}

class InventoryTest : public ::testing::Test {
protected:
    rrs_session_t admin{};
    rrs_session_t passenger{};

    void SetUp() override {
        auth_reset_for_tests();
        inventory_reset_for_tests();
        ASSERT_EQ(auth_create_user_for_tests("admin@rrs.test", "Admin1234", "Admin", RRS_ROLE_ADMIN), RRS_OK);
        ASSERT_EQ(auth_register("pax@rrs.test", "Pass1234", "Passenger"), RRS_OK);
        ASSERT_EQ(auth_login("admin@rrs.test", "Admin1234", &admin), RRS_OK);
        ASSERT_EQ(auth_login("pax@rrs.test", "Pass1234", &passenger), RRS_OK);
    }
};

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

/* ---------- FR-007: create train ---------- */

TEST_F(InventoryTest, AdminCreatesATrainAndCanReadItBack) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);

    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_STREQ(got.train_id, "12001");
    EXPECT_EQ(got.status, RRS_TRAIN_ACTIVE);
    EXPECT_EQ(got.halt_count, 3);
    EXPECT_STREQ(got.halts[1].station, "CNB");
    EXPECT_EQ(got.halts[2].distance_km, 510);
    EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], 100);
    EXPECT_EQ(got.capacity[RRS_CLASS_AC3], 50);
    EXPECT_EQ(got.capacity[RRS_CLASS_AC2], 20);
}

TEST_F(InventoryTest, CreatedTrainBecomesSearchable) {
    EXPECT_FALSE(searchable("NDLS", "LKO"));
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    EXPECT_TRUE(searchable("NDLS", "LKO"));
    EXPECT_TRUE(searchable("NDLS", "CNB"));   /* intermediate halt */
}

TEST_F(InventoryTest, ClassWithZeroCapacityIsAllowedWhileAnotherHasSeats) {
    rrs_train_t t = make_train("12001", good_route(), 0, 0, 40);
    EXPECT_EQ(train_create(&admin, &t), RRS_OK);
}

TEST_F(InventoryTest, PassengerCannotCreateATrain) {
    rrs_train_t t = make_train("12001", good_route());
    EXPECT_EQ(train_create(&passenger, &t), ERR_FORBIDDEN);
    EXPECT_EQ(inventory_train_count_for_tests(), 0);
    EXPECT_FALSE(searchable("NDLS", "LKO"));
}

TEST_F(InventoryTest, MissingForgedOrLoggedOutSessionCannotCreate) {
    rrs_train_t t = make_train("12001", good_route());
    EXPECT_EQ(train_create(nullptr, &t), ERR_SESSION_EXPIRED);

    rrs_session_t forged = admin;
    forged.token[0] = forged.token[0] == 'a' ? 'b' : 'a';
    EXPECT_EQ(train_create(&forged, &t), ERR_SESSION_EXPIRED);

    rrs_session_t escalated = passenger;     /* passenger editing their copy to say ADMIN */
    escalated.role = RRS_ROLE_ADMIN;
    EXPECT_EQ(train_create(&escalated, &t), ERR_FORBIDDEN);

    ASSERT_EQ(auth_logout(&admin), RRS_OK);
    EXPECT_EQ(train_create(&admin, &t), ERR_SESSION_EXPIRED);
    EXPECT_EQ(inventory_train_count_for_tests(), 0);
}

TEST_F(InventoryTest, DuplicateTrainIdIsRejectedAndOriginalKept) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);

    rrs_train_t other = make_train("12001", {halt("BBS", 100, 100, 0), halt("HWH", 400, 400, 440)}, 7, 7, 7);
    EXPECT_EQ(train_create(&admin, &other), ERR_DUPLICATE);

    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_STREQ(got.halts[0].station, "NDLS");
    EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], 100);
    EXPECT_EQ(inventory_train_count_for_tests(), 1);
}

TEST_F(InventoryTest, BadTrainIdsAreRejected) {
    EXPECT_EQ(train_create(&admin, nullptr), ERR_INVALID_INPUT);

    for (const char* bad : {"", "12 01", "12-01", "ELEVENCHARS"}) {
        rrs_train_t b = make_train("X", good_route());
        std::memset(b.train_id, 0, sizeof b.train_id);
        if (std::strlen(bad) >= RRS_TRAIN_ID_LEN) {
            std::memcpy(b.train_id, bad, RRS_TRAIN_ID_LEN);          /* no terminator */
        } else {
            std::strcpy(b.train_id, bad);
        }
        EXPECT_EQ(train_create(&admin, &b), ERR_INVALID_INPUT) << "id: '" << bad << "'";
    }
    EXPECT_EQ(inventory_train_count_for_tests(), 0);
}

TEST_F(InventoryTest, InvalidRouteIsRejectedAndNothingIsSaved) {
    rrs_train_t t = make_train("12001", good_route());
    t.halts[2].distance_km = t.halts[1].distance_km;     /* distance does not increase */
    EXPECT_EQ(train_create(&admin, &t), ERR_INVALID_INPUT);

    rrs_train_t one = make_train("12002", {halt("NDLS", 480, 480, 0)});
    EXPECT_EQ(train_create(&admin, &one), ERR_INVALID_INPUT);

    rrs_train_t bad_count = make_train("12003", good_route());
    bad_count.halt_count = RRS_MAX_HALTS + 1;
    EXPECT_EQ(train_create(&admin, &bad_count), ERR_INVALID_INPUT);

    EXPECT_EQ(inventory_train_count_for_tests(), 0);
    EXPECT_FALSE(searchable("NDLS", "LKO"));
    rrs_train_t got;
    EXPECT_EQ(train_get(&admin, "12001", &got), ERR_NOT_FOUND);
}

TEST_F(InventoryTest, BadCapacitiesAreRejected) {
    for (auto caps : std::vector<std::vector<int>>{
             {-1, 10, 10}, {10, -1, 10}, {10, 10, -1},
             {RRS_MAX_CLASS_CAPACITY + 1, 0, 0}, {0, 0, 0}}) {
        rrs_train_t t = make_train("12001", good_route(), caps[0], caps[1], caps[2]);
        EXPECT_EQ(train_create(&admin, &t), ERR_INVALID_INPUT);
    }
    EXPECT_EQ(inventory_train_count_for_tests(), 0);

    rrs_train_t max = make_train("12001", good_route(), RRS_MAX_CLASS_CAPACITY, 0, 0);
    EXPECT_EQ(train_create(&admin, &max), RRS_OK);
}

TEST_F(InventoryTest, CallerCannotChooseTheInitialStatus) {
    rrs_train_t t = make_train("12001", good_route());
    t.status = RRS_TRAIN_RETIRED;
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_EQ(got.status, RRS_TRAIN_ACTIVE);
    EXPECT_TRUE(searchable("NDLS", "LKO"));
}

TEST_F(InventoryTest, StoredTrainIsACopyOfTheCallersStruct) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    t.capacity[RRS_CLASS_SLEEPER] = 1;
    t.halts[0].distance_km = 99;
    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], 100);
    EXPECT_EQ(got.halts[0].distance_km, 0);
}

TEST_F(InventoryTest, FiveHundredTrainsAreSupported) {          /* NFR-008 */
    char id[RRS_TRAIN_ID_LEN];
    for (int i = 0; i < 600; ++i) {
        std::snprintf(id, sizeof id, "T%04d", (i * 7919) % 10000);   /* unsorted insertion order */
        rrs_train_t t = make_train(id, good_route());
        ASSERT_EQ(train_create(&admin, &t), RRS_OK) << id;
    }
    EXPECT_EQ(inventory_train_count_for_tests(), 600);
    rrs_train_t got;
    for (int i = 0; i < 600; ++i) {
        std::snprintf(id, sizeof id, "T%04d", (i * 7919) % 10000);
        ASSERT_EQ(train_get(&passenger, id, &got), RRS_OK) << id;
        EXPECT_STREQ(got.train_id, id);
    }
}

/* ---------- train_get ---------- */

TEST_F(InventoryTest, GetNeedsALiveSessionButNotAdmin) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    rrs_train_t got;
    EXPECT_EQ(train_get(&passenger, "12001", &got), RRS_OK);
    EXPECT_EQ(train_get(nullptr, "12001", &got), ERR_SESSION_EXPIRED);
    EXPECT_EQ(got.train_id[0], '\0');                   /* zeroed on failure */
}

TEST_F(InventoryTest, GetUnknownTrainOrNullArgumentsFail) {
    rrs_train_t got;
    EXPECT_EQ(train_get(&admin, "99999", &got), ERR_NOT_FOUND);
    EXPECT_EQ(train_get(&admin, nullptr, &got), ERR_INVALID_INPUT);
    EXPECT_EQ(train_get(&admin, "12001", nullptr), ERR_INVALID_INPUT);
}

/* ---------- FR-008: edit train ---------- */

TEST_F(InventoryTest, AdminEditsCapacitiesAndTheyAreStored) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);

    t.capacity[RRS_CLASS_SLEEPER] = 150;
    t.capacity[RRS_CLASS_AC3] = 0;
    t.capacity[RRS_CLASS_AC2] = 30;
    ASSERT_EQ(train_update(&admin, &t), RRS_OK);

    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], 150);
    EXPECT_EQ(got.capacity[RRS_CLASS_AC3], 0);
    EXPECT_EQ(got.capacity[RRS_CLASS_AC2], 30);
    EXPECT_EQ(got.status, RRS_TRAIN_ACTIVE);
    EXPECT_EQ(inventory_train_count_for_tests(), 1);
}

TEST_F(InventoryTest, EditedRouteReplacesTheOldOneInSearch) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    ASSERT_TRUE(searchable("NDLS", "LKO"));
    ASSERT_FALSE(searchable("NDLS", "BSB"));

    rrs_train_t edited = make_train("12001", {halt("NDLS", 480, 480, 0), halt("CNB", 720, 725, 440),
                                              halt("BSB", 900, 900, 760)});
    ASSERT_EQ(train_update(&admin, &edited), RRS_OK);

    EXPECT_TRUE(searchable("NDLS", "BSB"));
    EXPECT_TRUE(searchable("NDLS", "CNB"));
    EXPECT_FALSE(searchable("NDLS", "LKO"));    /* halt removed from the route */

    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_EQ(got.halt_count, 3);
    EXPECT_STREQ(got.halts[2].station, "BSB");
}

TEST_F(InventoryTest, EditedTimingsShowUpInSearchResults) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);

    rrs_train_t edited = make_train("12001", {halt("NDLS", 600, 600, 0), halt("CNB", 840, 845, 440),
                                              halt("LKO", 960, 960, 510)});
    ASSERT_EQ(train_update(&admin, &edited), RRS_OK);

    rrs_route_list_t out;
    ASSERT_EQ(search_routes("NDLS", "LKO", kTravelDate, nullptr, &out), RRS_OK);
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.routes[0].legs[0].depart_min, 600);
    EXPECT_EQ(out.routes[0].legs[0].arrive_min, 960);
}

TEST_F(InventoryTest, EditingOneTrainLeavesOthersUntouched) {
    rrs_train_t a = make_train("12001", good_route());
    rrs_train_t b = make_train("12002", {halt("BBS", 100, 100, 0), halt("HWH", 400, 400, 440)}, 10, 10, 10);
    ASSERT_EQ(train_create(&admin, &a), RRS_OK);
    ASSERT_EQ(train_create(&admin, &b), RRS_OK);

    a.capacity[RRS_CLASS_SLEEPER] = 5;
    ASSERT_EQ(train_update(&admin, &a), RRS_OK);

    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12002", &got), RRS_OK);
    EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], 10);
    EXPECT_TRUE(searchable("BBS", "HWH"));
    EXPECT_TRUE(searchable("NDLS", "LKO"));
}

TEST_F(InventoryTest, PassengerCannotEditATrain) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    t.capacity[RRS_CLASS_SLEEPER] = 1;
    EXPECT_EQ(train_update(&passenger, &t), ERR_FORBIDDEN);

    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], 100);
}

TEST_F(InventoryTest, MissingOrForgedSessionCannotEdit) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    t.capacity[RRS_CLASS_SLEEPER] = 1;
    EXPECT_EQ(train_update(nullptr, &t), ERR_SESSION_EXPIRED);

    rrs_session_t forged = admin;
    forged.token[0] = forged.token[0] == 'a' ? 'b' : 'a';
    EXPECT_EQ(train_update(&forged, &t), ERR_SESSION_EXPIRED);

    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], 100);
}

TEST_F(InventoryTest, EditingAnUnknownTrainFails) {
    rrs_train_t t = make_train("12001", good_route());
    EXPECT_EQ(train_update(&admin, &t), ERR_NOT_FOUND);
    EXPECT_EQ(inventory_train_count_for_tests(), 0);
}

TEST_F(InventoryTest, InvalidEditIsRejectedAndOldScheduleKept) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);

    rrs_train_t bad_route = make_train("12001", good_route());
    bad_route.halts[1].distance_km = 0;
    EXPECT_EQ(train_update(&admin, &bad_route), ERR_INVALID_INPUT);

    rrs_train_t bad_caps = make_train("12001", good_route(), 0, 0, 0);
    EXPECT_EQ(train_update(&admin, &bad_caps), ERR_INVALID_INPUT);

    EXPECT_EQ(train_update(&admin, nullptr), ERR_INVALID_INPUT);

    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], 100);
    EXPECT_EQ(got.halts[1].distance_km, 440);
    EXPECT_TRUE(searchable("NDLS", "LKO"));
}

TEST_F(InventoryTest, EditCannotChangeStatus) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    t.status = RRS_TRAIN_RETIRED;
    ASSERT_EQ(train_update(&admin, &t), RRS_OK);

    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_EQ(got.status, RRS_TRAIN_ACTIVE);
    EXPECT_TRUE(searchable("NDLS", "LKO"));
}

/* ---------- FR-009: retire train ---------- */

TEST_F(InventoryTest, RetiredTrainIsNoLongerBookableOrSearchable) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    ASSERT_TRUE(train_is_bookable("12001"));
    ASSERT_TRUE(searchable("NDLS", "LKO"));

    ASSERT_EQ(train_retire(&admin, "12001"), RRS_OK);
    EXPECT_FALSE(train_is_bookable("12001"));
    EXPECT_FALSE(searchable("NDLS", "LKO"));
}

TEST_F(InventoryTest, RetiredTrainRecordIsPreserved) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    ASSERT_EQ(train_retire(&admin, "12001"), RRS_OK);

    rrs_train_t got;
    ASSERT_EQ(train_get(&passenger, "12001", &got), RRS_OK);
    EXPECT_EQ(got.status, RRS_TRAIN_RETIRED);
    EXPECT_EQ(got.halt_count, 3);
    EXPECT_STREQ(got.halts[0].station, "NDLS");
    EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], 100);
    EXPECT_EQ(inventory_train_count_for_tests(), 1);
}

TEST_F(InventoryTest, RetiringOneTrainLeavesOthersBookable) {
    rrs_train_t a = make_train("12001", good_route());
    rrs_train_t b = make_train("12002", {halt("NDLS", 500, 500, 0), halt("LKO", 900, 900, 510)});
    ASSERT_EQ(train_create(&admin, &a), RRS_OK);
    ASSERT_EQ(train_create(&admin, &b), RRS_OK);

    ASSERT_EQ(train_retire(&admin, "12001"), RRS_OK);
    EXPECT_FALSE(train_is_bookable("12001"));
    EXPECT_TRUE(train_is_bookable("12002"));

    rrs_route_list_t out;
    ASSERT_EQ(search_routes("NDLS", "LKO", kTravelDate, nullptr, &out), RRS_OK);
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.routes[0].legs[0].train_id, "12002");
}

TEST_F(InventoryTest, RetireIsRepeatableAndIdUnusable) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    EXPECT_EQ(train_retire(&admin, "12001"), RRS_OK);
    EXPECT_EQ(train_retire(&admin, "12001"), RRS_OK);        /* already retired: no change */
    EXPECT_EQ(train_create(&admin, &t), ERR_DUPLICATE);      /* id stays reserved */
    EXPECT_FALSE(train_is_bookable("12001"));
}

TEST_F(InventoryTest, RetiredTrainCannotBeEdited) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    ASSERT_EQ(train_retire(&admin, "12001"), RRS_OK);

    t.capacity[RRS_CLASS_SLEEPER] = 5;
    EXPECT_EQ(train_update(&admin, &t), ERR_INVALID_INPUT);
    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], 100);
    EXPECT_EQ(got.status, RRS_TRAIN_RETIRED);
    EXPECT_FALSE(searchable("NDLS", "LKO"));
}

TEST_F(InventoryTest, PassengerCannotRetireATrain) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    EXPECT_EQ(train_retire(&passenger, "12001"), ERR_FORBIDDEN);
    EXPECT_TRUE(train_is_bookable("12001"));
    EXPECT_TRUE(searchable("NDLS", "LKO"));
}

TEST_F(InventoryTest, MissingOrForgedSessionCannotRetire) {
    rrs_train_t t = make_train("12001", good_route());
    ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    EXPECT_EQ(train_retire(nullptr, "12001"), ERR_SESSION_EXPIRED);

    rrs_session_t forged = admin;
    forged.token[0] = forged.token[0] == 'a' ? 'b' : 'a';
    EXPECT_EQ(train_retire(&forged, "12001"), ERR_SESSION_EXPIRED);
    EXPECT_TRUE(train_is_bookable("12001"));
}

TEST_F(InventoryTest, RetireUnknownTrainOrNullIdFails) {
    EXPECT_EQ(train_retire(&admin, "99999"), ERR_NOT_FOUND);
    EXPECT_EQ(train_retire(&admin, nullptr), ERR_INVALID_INPUT);
}

TEST_F(InventoryTest, UnknownOrNullTrainIsNotBookable) {
    EXPECT_FALSE(train_is_bookable("99999"));
    EXPECT_FALSE(train_is_bookable(nullptr));
}

/* ---------- FR-011: capacity reduction guard ---------- */

namespace {

/* Stand-in for the booking module's confirmed-booking counter. */
int g_confirmed[RRS_CLASS_COUNT];
int g_counter_calls;
std::string g_counter_last_train;

int fake_confirmed_count(const char* train_id, rrs_class_t cls) {
    ++g_counter_calls;
    g_counter_last_train = train_id;
    return std::strcmp(train_id, "12001") == 0 ? g_confirmed[cls] : 0;
}

class CapacityGuardTest : public InventoryTest {
protected:
    void SetUp() override {
        InventoryTest::SetUp();
        for (int& c : g_confirmed) c = 0;
        g_counter_calls = 0;
        g_counter_last_train.clear();
        inventory_set_booking_counter(fake_confirmed_count);
        rrs_train_t t = make_train("12001", good_route(), 100, 50, 20);
        ASSERT_EQ(train_create(&admin, &t), RRS_OK);
    }

    rrs_status_t set_capacity(int sleeper, int ac3, int ac2) {
        rrs_train_t t = make_train("12001", good_route(), sleeper, ac3, ac2);
        return train_update(&admin, &t);
    }

    void expect_capacity(int sleeper, int ac3, int ac2) {
        rrs_train_t got;
        ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
        EXPECT_EQ(got.capacity[RRS_CLASS_SLEEPER], sleeper);
        EXPECT_EQ(got.capacity[RRS_CLASS_AC3], ac3);
        EXPECT_EQ(got.capacity[RRS_CLASS_AC2], ac2);
    }
};

}  // namespace

TEST_F(CapacityGuardTest, ReductionBelowConfirmedBookingsIsRejected) {
    g_confirmed[RRS_CLASS_SLEEPER] = 80;
    EXPECT_EQ(set_capacity(79, 50, 20), ERR_CAPACITY_CONFLICT);
    expect_capacity(100, 50, 20);
}

TEST_F(CapacityGuardTest, ReductionDownToConfirmedBookingsIsAllowed) {
    g_confirmed[RRS_CLASS_SLEEPER] = 80;
    EXPECT_EQ(set_capacity(80, 50, 20), RRS_OK);
    expect_capacity(80, 50, 20);
}

TEST_F(CapacityGuardTest, EveryClassIsChecked) {
    g_confirmed[RRS_CLASS_AC3] = 30;
    EXPECT_EQ(set_capacity(100, 29, 20), ERR_CAPACITY_CONFLICT);
    g_confirmed[RRS_CLASS_AC3] = 0;
    g_confirmed[RRS_CLASS_AC2] = 20;
    EXPECT_EQ(set_capacity(100, 50, 19), ERR_CAPACITY_CONFLICT);
    expect_capacity(100, 50, 20);
}

TEST_F(CapacityGuardTest, ConflictRejectsTheWholeUpdateIncludingRouteChanges) {
    g_confirmed[RRS_CLASS_AC2] = 15;
    rrs_train_t t = make_train("12001", {halt("NDLS", 600, 600, 0), halt("BSB", 900, 900, 760)},
                               500, 500, 10);              /* raises two classes, cuts AC2 */
    EXPECT_EQ(train_update(&admin, &t), ERR_CAPACITY_CONFLICT);

    expect_capacity(100, 50, 20);
    rrs_train_t got;
    ASSERT_EQ(train_get(&admin, "12001", &got), RRS_OK);
    EXPECT_EQ(got.halt_count, 3);
    EXPECT_TRUE(searchable("NDLS", "LKO"));
    EXPECT_FALSE(searchable("NDLS", "BSB"));
}

TEST_F(CapacityGuardTest, IncreasingCapacityIsAlwaysAllowed) {
    g_confirmed[RRS_CLASS_SLEEPER] = 100;
    EXPECT_EQ(set_capacity(300, 50, 20), RRS_OK);
    expect_capacity(300, 50, 20);
}

TEST_F(CapacityGuardTest, DroppingAClassToZeroNeedsNoConfirmedBookingsInIt) {
    EXPECT_EQ(set_capacity(100, 0, 20), RRS_OK);
    g_confirmed[RRS_CLASS_SLEEPER] = 1;
    EXPECT_EQ(set_capacity(0, 0, 20), ERR_CAPACITY_CONFLICT);
    expect_capacity(100, 0, 20);
}

TEST_F(CapacityGuardTest, RouteEditsAreAllowedWhileBookingsExist) {
    g_confirmed[RRS_CLASS_SLEEPER] = 100;
    g_confirmed[RRS_CLASS_AC3] = 50;
    g_confirmed[RRS_CLASS_AC2] = 20;
    rrs_train_t t = make_train("12001", {halt("NDLS", 600, 600, 0), halt("BSB", 900, 900, 760)},
                               100, 50, 20);
    EXPECT_EQ(train_update(&admin, &t), RRS_OK);
    EXPECT_TRUE(searchable("NDLS", "BSB"));
}

TEST_F(CapacityGuardTest, OnlyThisTrainsBookingsCount) {
    rrs_train_t other = make_train("12002", {halt("BBS", 100, 100, 0), halt("HWH", 400, 400, 440)}, 10, 10, 10);
    ASSERT_EQ(train_create(&admin, &other), RRS_OK);
    g_confirmed[RRS_CLASS_SLEEPER] = 100;                 /* belongs to 12001 */
    other.capacity[RRS_CLASS_SLEEPER] = 5;
    EXPECT_EQ(train_update(&admin, &other), RRS_OK);
    EXPECT_EQ(g_counter_last_train, "12002");
}

TEST_F(CapacityGuardTest, WithoutACounterNothingIsConfirmed) {
    inventory_set_booking_counter(nullptr);
    g_confirmed[RRS_CLASS_SLEEPER] = 100;
    EXPECT_EQ(set_capacity(1, 1, 1), RRS_OK);
    expect_capacity(1, 1, 1);
}

TEST_F(CapacityGuardTest, GuardRunsAfterSessionAndInputChecks) {
    g_confirmed[RRS_CLASS_SLEEPER] = 80;
    rrs_train_t t = make_train("12001", good_route(), 10, 50, 20);
    EXPECT_EQ(train_update(&passenger, &t), ERR_FORBIDDEN);
    EXPECT_EQ(train_update(nullptr, &t), ERR_SESSION_EXPIRED);
    EXPECT_EQ(g_counter_calls, 0);                        /* never consulted */

    t.halts[1].distance_km = 0;                           /* invalid route and a conflict */
    EXPECT_EQ(train_update(&admin, &t), ERR_INVALID_INPUT);
    expect_capacity(100, 50, 20);
}

TEST_F(CapacityGuardTest, RetireIsNotBlockedByConfirmedBookings) {
    g_confirmed[RRS_CLASS_SLEEPER] = 100;
    EXPECT_EQ(train_retire(&admin, "12001"), RRS_OK);
    expect_capacity(100, 50, 20);                         /* history kept as it was */
}
