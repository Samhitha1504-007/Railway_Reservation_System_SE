/*
 * SEC-006: audit logging. Test Plan 5.1: "trigger 3 failed logins and an admin
 * action; confirm audit-log entries". Design doc 4.4: entries carry timestamp,
 * user id and function name; never passwords, tokens or payment details.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "audit.h"
#include "audit_internal.h"
#include "auth.h"
#include "auth_internal.h"
#include "session.h"

namespace {

class AuditTest : public ::testing::Test {
protected:
    void SetUp() override {
        auth_reset_for_tests();
        audit_reset_for_tests();
        ASSERT_EQ(RRS_OK, auth_register("rider@example.com", "Passw0rdOK", "Rider"));
        ASSERT_EQ(RRS_OK, auth_create_user_for_tests("admin@example.com", "Adm1nPassw0rd",
                                                      "Admin", RRS_ROLE_ADMIN));
    }
    void TearDown() override {
        audit_reset_for_tests();
        rrs::session_set_fixed_time_for_tests(0);
    }

    std::vector<std::string> entries() {
        std::vector<std::string> out;
        std::string line;
        for (size_t i = 0; audit_entry_for_tests(i, line); ++i) out.push_back(line);
        return out;
    }
    static bool has(const std::string& line, const char* text) {
        return line.find(text) != std::string::npos;
    }
    rrs_session_t login(const char* email, const char* pw) {
        rrs_session_t s;
        EXPECT_EQ(RRS_OK, auth_login(email, pw, &s));
        return s;
    }
};

}  // namespace

/* ---- the Test Plan scenario ---- */

TEST_F(AuditTest, ThreeFailedLoginsAndAnAdminActionAreAllLogged) {
    rrs_session_t bad;
    EXPECT_EQ(ERR_AUTH_FAILED, auth_login("rider@example.com", "Wr0ngPass1", &bad));
    EXPECT_EQ(ERR_AUTH_FAILED, auth_login("rider@example.com", "Wr0ngPass2", &bad));
    EXPECT_EQ(ERR_AUTH_FAILED, auth_login("nobody@example.com", "Wr0ngPass3", &bad));

    rrs_session_t admin = login("admin@example.com", "Adm1nPassw0rd");
    EXPECT_EQ(RRS_OK, auth_authorize_for(&admin, RRS_ROLE_ADMIN, "train_create"));

    std::vector<std::string> e = entries();
    ASSERT_EQ(4u, e.size());
    EXPECT_TRUE(has(e[0], "event=LOGIN_FAILED"));
    EXPECT_TRUE(has(e[1], "event=LOGIN_FAILED"));
    EXPECT_TRUE(has(e[2], "event=LOGIN_FAILED"));
    EXPECT_TRUE(has(e[3], "event=ADMIN_ACTION"));
    EXPECT_TRUE(has(e[3], "fn=train_create"));
}

/* ---- what an entry contains ---- */

TEST_F(AuditTest, FailedLoginEntryHasTimestampUserFunctionAndStatus) {
    audit_set_fixed_time_for_tests(1700000000);               /* 2023-11-14 22:13:20 UTC */
    rrs_session_t bad;
    ASSERT_EQ(ERR_AUTH_FAILED, auth_login("rider@example.com", "Wr0ngPass1", &bad));
    std::vector<std::string> e = entries();
    ASSERT_EQ(1u, e.size());
    EXPECT_EQ(0u, e[0].find("2023-11-14T22:13:20Z "));
    EXPECT_TRUE(has(e[0], " event=LOGIN_FAILED "));
    EXPECT_TRUE(has(e[0], " user=rider@example.com "));
    EXPECT_TRUE(has(e[0], " fn=auth_login "));
    EXPECT_TRUE(has(e[0], " status=ERR_AUTH_FAILED"));
}

TEST_F(AuditTest, SuccessfulLoginIsNotLogged) {
    login("rider@example.com", "Passw0rdOK");
    EXPECT_EQ(0u, audit_entry_count_for_tests());
}

TEST_F(AuditTest, ForbiddenAttemptIsLoggedWithTheServersUserIdAndFunction) {
    rrs_session_t pax = login("rider@example.com", "Passw0rdOK");
    EXPECT_EQ(ERR_FORBIDDEN, auth_authorize_for(&pax, RRS_ROLE_ADMIN, "train_create"));
    std::vector<std::string> e = entries();
    ASSERT_EQ(1u, e.size());
    EXPECT_TRUE(has(e[0], "event=ACCESS_DENIED"));
    EXPECT_TRUE(has(e[0], (std::string("user=") + pax.user_id).c_str()));
    EXPECT_TRUE(has(e[0], "fn=train_create"));
    EXPECT_TRUE(has(e[0], "status=ERR_FORBIDDEN"));
}

TEST_F(AuditTest, ForgedUserIdInTheCallersCopyIsNeverWrittenToTheLog) {
    rrs_session_t pax = login("rider@example.com", "Passw0rdOK");
    rrs_session_t forged = pax;
    std::strncpy(forged.user_id, "evil-user-id", RRS_USER_ID_LEN - 1);
    EXPECT_EQ(ERR_SESSION_EXPIRED, auth_authorize_for(&forged, RRS_ROLE_ADMIN, "train_create"));
    std::vector<std::string> e = entries();
    ASSERT_EQ(1u, e.size());
    EXPECT_TRUE(has(e[0], "event=SESSION_REJECTED"));
    EXPECT_TRUE(has(e[0], "user=unknown"));
    EXPECT_FALSE(has(e[0], "evil-user-id"));
}

TEST_F(AuditTest, PlainAuthorizeLogsRejectedSessionsButNotWrongRoles) {
    rrs_session_t pax = login("rider@example.com", "Passw0rdOK");
    EXPECT_EQ(ERR_FORBIDDEN, auth_authorize(&pax, RRS_ROLE_ADMIN));    /* e.g. "admin or passenger" read */
    EXPECT_EQ(RRS_OK, auth_authorize(&pax, RRS_ROLE_PASSENGER));
    EXPECT_EQ(0u, audit_entry_count_for_tests());

    rrs_session_t fake = pax;
    std::memset(fake.token, 'a', RRS_TOKEN_LEN - 1);
    fake.token[RRS_TOKEN_LEN - 1] = '\0';
    EXPECT_EQ(ERR_SESSION_EXPIRED, auth_authorize(&fake, RRS_ROLE_PASSENGER));
    std::vector<std::string> e = entries();
    ASSERT_EQ(1u, e.size());
    EXPECT_TRUE(has(e[0], "event=SESSION_REJECTED"));
    EXPECT_TRUE(has(e[0], "fn=auth_authorize"));
}

TEST_F(AuditTest, PassengerSuccessIsNotLoggedAdminSuccessIs) {
    rrs_session_t pax = login("rider@example.com", "Passw0rdOK");
    EXPECT_EQ(RRS_OK, auth_authorize_for(&pax, RRS_ROLE_PASSENGER, "booking_create"));
    EXPECT_EQ(0u, audit_entry_count_for_tests());

    rrs_session_t admin = login("admin@example.com", "Adm1nPassw0rd");
    EXPECT_EQ(RRS_OK, auth_authorize_for(&admin, RRS_ROLE_ADMIN, "train_update"));
    EXPECT_EQ(1u, audit_entry_count_for_tests());
}

TEST_F(AuditTest, DeadSessionsOnProfileAndLogoutAreLoggedWithTheirFunctionNames) {
    rrs_session_t gone = login("rider@example.com", "Passw0rdOK");
    ASSERT_EQ(RRS_OK, auth_logout(&gone));
    rrs_profile_t p;
    EXPECT_EQ(ERR_SESSION_EXPIRED, auth_profile_get(&gone, &p));
    EXPECT_EQ(ERR_SESSION_EXPIRED, auth_profile_update(&gone, "X Y", nullptr));
    EXPECT_EQ(ERR_SESSION_EXPIRED, auth_logout(&gone));
    std::vector<std::string> e = entries();
    ASSERT_EQ(3u, e.size());
    EXPECT_TRUE(has(e[0], "fn=auth_profile_get"));
    EXPECT_TRUE(has(e[1], "fn=auth_profile_update"));
    EXPECT_TRUE(has(e[2], "fn=auth_logout"));
}

/* ---- what must never be in the log ---- */

TEST_F(AuditTest, NoPasswordOrTokenEverAppearsInTheLog) {
    rrs_session_t bad;
    auth_login("rider@example.com", "Wr0ngPassw0rd!", &bad);
    auth_login("nobody@example.com", "Secret-Guess-9", &bad);

    rrs_session_t pax = login("rider@example.com", "Passw0rdOK");
    std::string token = pax.token;
    auth_authorize_for(&pax, RRS_ROLE_ADMIN, "train_create");
    auth_logout(&pax);
    auth_authorize(&pax, RRS_ROLE_PASSENGER);                          /* reuse after logout */

    ASSERT_GE(audit_entry_count_for_tests(), 4u);
    for (const std::string& line : entries()) {
        EXPECT_FALSE(has(line, "Wr0ngPassw0rd!")) << line;
        EXPECT_FALSE(has(line, "Secret-Guess-9")) << line;
        EXPECT_FALSE(has(line, "Passw0rdOK")) << line;
        EXPECT_FALSE(has(line, token.c_str())) << line;
    }
}

/* ---- the log cannot be forged or flooded ---- */

TEST_F(AuditTest, NewlinesAndFakeFieldsInAnAttemptedEmailCannotForgeLogLines) {
    rrs_session_t bad;
    auth_login("x\nevent=ADMIN_ACTION user=root fn=train_create", "Wr0ngPass1", &bad);
    auth_login("y\r\n2026-01-01T00:00:00Z event=ADMIN_ACTION", "Wr0ngPass1", &bad);
    std::vector<std::string> e = entries();
    ASSERT_EQ(2u, e.size());
    for (const std::string& line : e) {
        EXPECT_EQ(std::string::npos, line.find('\n')) << line;
        EXPECT_EQ(std::string::npos, line.find('\r')) << line;
        EXPECT_TRUE(has(line, "event=LOGIN_FAILED")) << line;
        EXPECT_FALSE(has(line, "event=ADMIN_ACTION")) << line;
    }
}

TEST_F(AuditTest, AVeryLongEmailIsCutInTheLog) {
    rrs_session_t bad;
    auth_login(std::string(10000, 'a').c_str(), "Wr0ngPass1", &bad);
    std::vector<std::string> e = entries();
    ASSERT_EQ(1u, e.size());
    EXPECT_LT(e[0].size(), 250u);
}

TEST_F(AuditTest, OnlyTheMostRecentEntriesAreKeptInMemory) {
    rrs_session_t bad;
    for (size_t i = 0; i < kAuditMemoryEntries + 20; ++i)
        audit_event("TEST", "u", "f", ERR_AUTH_FAILED);
    EXPECT_EQ(kAuditMemoryEntries, audit_entry_count_for_tests());
    (void)bad;
}

/* ---- the file sink ---- */

TEST_F(AuditTest, EventsAreAppendedToTheLogFile) {
    std::string path = ::testing::TempDir() + "rrs_audit_test.log";
    std::remove(path.c_str());
    ASSERT_EQ(RRS_OK, audit_init(path.c_str()));

    rrs_session_t bad;
    auth_login("rider@example.com", "Wr0ngPass1", &bad);
    auth_login("rider@example.com", "Wr0ngPass2", &bad);
    audit_close();

    std::ifstream in(path);
    std::string line;
    int count = 0;
    while (std::getline(in, line)) {
        EXPECT_TRUE(has(line, "event=LOGIN_FAILED")) << line;
        ++count;
    }
    EXPECT_EQ(2, count);
    std::remove(path.c_str());
}

TEST_F(AuditTest, ReopeningTheFileKeepsEarlierLines) {
    std::string path = ::testing::TempDir() + "rrs_audit_test2.log";
    std::remove(path.c_str());
    rrs_session_t bad;
    ASSERT_EQ(RRS_OK, audit_init(path.c_str()));
    auth_login("rider@example.com", "Wr0ngPass1", &bad);
    audit_close();
    ASSERT_EQ(RRS_OK, audit_init(path.c_str()));
    auth_login("rider@example.com", "Wr0ngPass2", &bad);
    audit_close();

    std::ifstream in(path);
    std::string line;
    int count = 0;
    while (std::getline(in, line)) ++count;
    EXPECT_EQ(2, count);
    std::remove(path.c_str());
}

TEST_F(AuditTest, AnUnwritableLogFileReportsErrIoButNeverBreaksTheCaller) {
    EXPECT_EQ(ERR_IO, audit_init("/nonexistent-dir-for-rrs/audit.log"));
    rrs_session_t bad;
    EXPECT_EQ(ERR_AUTH_FAILED, auth_login("rider@example.com", "Wr0ngPass1", &bad));
    EXPECT_EQ(1u, audit_entry_count_for_tests());                     /* still kept in memory */
}

TEST_F(AuditTest, NullFieldsAreLoggedAsDashes) {
    EXPECT_EQ(RRS_OK, audit_event("TEST", nullptr, nullptr, ERR_IO));
    std::vector<std::string> e = entries();
    ASSERT_EQ(1u, e.size());
    EXPECT_TRUE(has(e[0], "user=- fn=- status=ERR_IO"));
}
