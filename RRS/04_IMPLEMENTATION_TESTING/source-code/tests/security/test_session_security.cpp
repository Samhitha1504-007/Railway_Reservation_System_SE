/*
 * SEC-004: expired or invalid session credentials are rejected by every
 * protected call with ERR_SESSION_EXPIRED (Test Plan 5.1: "reuse an
 * expired/invalid session token; expect ERR_SESSION_EXPIRED").
 */
#include <gtest/gtest.h>

#include <cstring>
#include <functional>
#include <vector>

#include "auth.h"
#include "auth_internal.h"
#include "session.h"

namespace {

struct Call {
    const char* name;
    std::function<rrs_status_t(const rrs_session_t*)> fn;
};

/* Every function that takes a session. */
std::vector<Call> protected_calls() {
    return {
        {"auth_authorize(ADMIN)",
         [](const rrs_session_t* s) { return auth_authorize(s, RRS_ROLE_ADMIN); }},
        {"auth_authorize(PASSENGER)",
         [](const rrs_session_t* s) { return auth_authorize(s, RRS_ROLE_PASSENGER); }},
        {"auth_profile_get",
         [](const rrs_session_t* s) { rrs_profile_t p; return auth_profile_get(s, &p); }},
        {"auth_profile_update",
         [](const rrs_session_t* s) { return auth_profile_update(s, "New Name", nullptr); }},
        {"auth_logout", [](const rrs_session_t* s) { return auth_logout(s); }},
    };
}

class SessionSecurityTest : public ::testing::Test {
protected:
    const time_t t0 = 7000000;

    void SetUp() override {
        auth_reset_for_tests();
        ASSERT_EQ(RRS_OK, auth_register("rider@example.com", "Passw0rdOK", "Rider"));
        ASSERT_EQ(RRS_OK, auth_register("other@example.com", "Passw0rdOK", "Other"));
    }
    void TearDown() override { rrs::session_set_fixed_time_for_tests(0); }

    rrs_session_t login(const char* email) {
        rrs_session_t s;
        EXPECT_EQ(RRS_OK, auth_login(email, "Passw0rdOK", &s));
        return s;
    }
    /* a session that has been idle past the timeout */
    rrs_session_t expired_session() {
        rrs::session_set_fixed_time_for_tests(t0);
        rrs_session_t s = login("rider@example.com");
        rrs::session_set_fixed_time_for_tests(t0 + rrs::kSessionTtlSeconds);
        return s;
    }
};

}  // namespace

TEST_F(SessionSecurityTest, ControlLiveSessionIsAcceptedByTheCallsThatShouldAcceptIt) {
    rrs_session_t s = login("rider@example.com");
    EXPECT_EQ(RRS_OK, auth_authorize(&s, RRS_ROLE_PASSENGER));
    rrs_profile_t p;
    EXPECT_EQ(RRS_OK, auth_profile_get(&s, &p));
    EXPECT_EQ(RRS_OK, auth_profile_update(&s, "New Name", nullptr));
    EXPECT_EQ(ERR_FORBIDDEN, auth_authorize(&s, RRS_ROLE_ADMIN));   /* live, wrong role */
}

TEST_F(SessionSecurityTest, ExpiredTokenIsRejectedByEveryProtectedCall) {
    for (const Call& c : protected_calls()) {
        SCOPED_TRACE(c.name);
        auth_reset_for_tests();
        ASSERT_EQ(RRS_OK, auth_register("rider@example.com", "Passw0rdOK", "Rider"));
        rrs_session_t s = expired_session();
        EXPECT_EQ(ERR_SESSION_EXPIRED, c.fn(&s));
    }
}

TEST_F(SessionSecurityTest, ExpiredTokenStaysRejectedWhenReused) {
    rrs_session_t s = expired_session();
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(ERR_SESSION_EXPIRED, auth_authorize(&s, RRS_ROLE_PASSENGER));
    }
}

TEST_F(SessionSecurityTest, ExpiredTokenIsNotRevivedWhenTheUserLogsInAgain) {
    rrs_session_t old = expired_session();
    rrs_session_t fresh = login("rider@example.com");
    EXPECT_EQ(RRS_OK, auth_authorize(&fresh, RRS_ROLE_PASSENGER));
    EXPECT_EQ(ERR_SESSION_EXPIRED, auth_authorize(&old, RRS_ROLE_PASSENGER));
}

TEST_F(SessionSecurityTest, ExpiredSessionIsReportedAsExpiredNeverForbidden) {
    rrs_session_t s = expired_session();
    EXPECT_EQ(ERR_SESSION_EXPIRED, auth_authorize(&s, RRS_ROLE_ADMIN));
}

TEST_F(SessionSecurityTest, MadeUpTokenIsRejectedByEveryProtectedCall) {
    for (const Call& c : protected_calls()) {
        SCOPED_TRACE(c.name);
        rrs_session_t s = login("rider@example.com");
        std::memset(s.token, 'f', RRS_TOKEN_LEN - 1);
        s.token[RRS_TOKEN_LEN - 1] = '\0';
        EXPECT_EQ(ERR_SESSION_EXPIRED, c.fn(&s));
    }
}

TEST_F(SessionSecurityTest, EmptyAndUnterminatedTokensAreRejectedByEveryProtectedCall) {
    for (const Call& c : protected_calls()) {
        SCOPED_TRACE(c.name);
        rrs_session_t empty;
        std::memset(&empty, 0, sizeof empty);
        EXPECT_EQ(ERR_SESSION_EXPIRED, c.fn(&empty));

        rrs_session_t unterminated = login("rider@example.com");
        std::memset(unterminated.token, 'a', RRS_TOKEN_LEN);            /* no NUL anywhere */
        EXPECT_EQ(ERR_SESSION_EXPIRED, c.fn(&unterminated));
    }
}

TEST_F(SessionSecurityTest, NullSessionIsRejectedByEveryProtectedCall) {
    for (const Call& c : protected_calls()) {
        SCOPED_TRACE(c.name);
        EXPECT_EQ(ERR_SESSION_EXPIRED, c.fn(nullptr));
    }
}

TEST_F(SessionSecurityTest, TokenIsDeadAfterLogoutForEveryProtectedCall) {
    for (const Call& c : protected_calls()) {
        SCOPED_TRACE(c.name);
        rrs_session_t s = login("rider@example.com");
        ASSERT_EQ(RRS_OK, auth_logout(&s));
        EXPECT_EQ(ERR_SESSION_EXPIRED, c.fn(&s));
    }
}

TEST_F(SessionSecurityTest, TokenWithEditedUserIdIsRejectedByEveryProtectedCall) {
    for (const Call& c : protected_calls()) {
        SCOPED_TRACE(c.name);
        rrs_session_t s = login("rider@example.com");
        std::strncpy(s.user_id, "00000000-0000-0000-0000-000000000000", RRS_USER_ID_LEN - 1);
        EXPECT_EQ(ERR_SESSION_EXPIRED, c.fn(&s));
    }
}

TEST_F(SessionSecurityTest, OneUsersTokenCannotActAsAnotherUser) {
    rrs_session_t mine = login("rider@example.com");
    rrs_session_t theirs = login("other@example.com");
    rrs_session_t swapped = mine;                                      /* my token, their user id */
    std::strncpy(swapped.user_id, theirs.user_id, RRS_USER_ID_LEN - 1);
    rrs_profile_t p;
    EXPECT_EQ(ERR_SESSION_EXPIRED, auth_profile_get(&swapped, &p));
    EXPECT_EQ(ERR_SESSION_EXPIRED, auth_profile_update(&swapped, "Hijacked", nullptr));
    ASSERT_EQ(RRS_OK, auth_profile_get(&theirs, &p));
    EXPECT_STREQ("Other", p.name);                                     /* their profile untouched */
}

TEST_F(SessionSecurityTest, ARejectedCallLeavesTheRealSessionWorking) {
    rrs_session_t s = login("rider@example.com");
    rrs_session_t fake = s;
    std::memset(fake.token, '0', RRS_TOKEN_LEN - 1);
    fake.token[RRS_TOKEN_LEN - 1] = '\0';
    EXPECT_EQ(ERR_SESSION_EXPIRED, auth_authorize(&fake, RRS_ROLE_PASSENGER));
    EXPECT_EQ(RRS_OK, auth_authorize(&s, RRS_ROLE_PASSENGER));
}
