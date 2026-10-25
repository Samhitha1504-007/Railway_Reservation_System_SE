#include <gtest/gtest.h>

#include <cstring>
#include <ctime>

#include "auth.h"
#include "auth_internal.h"
#include "password.h"
#include "session.h"

class AuthTest : public ::testing::Test {
protected:
    void SetUp() override { auth_reset_for_tests(); }
};

/* ---------- SEC-001: password hashing ---------- */

TEST(PasswordTest, HashIsNotPlaintextAndVerifies) {
    std::string enc;
    ASSERT_TRUE(rrs::password_hash("Secret123", enc));
    EXPECT_EQ(enc.find("Secret123"), std::string::npos);
    EXPECT_TRUE(rrs::password_verify("Secret123", enc));
}

TEST(PasswordTest, WrongPasswordFailsVerify) {
    std::string enc;
    ASSERT_TRUE(rrs::password_hash("Secret123", enc));
    EXPECT_FALSE(rrs::password_verify("Secret124", enc));
}

TEST(PasswordTest, SamePasswordGetsDifferentSalts) {
    std::string a, b;
    ASSERT_TRUE(rrs::password_hash("Secret123", a));
    ASSERT_TRUE(rrs::password_hash("Secret123", b));
    EXPECT_NE(a, b);
}

TEST(PasswordTest, MalformedEncodedStringIsRejected) {
    EXPECT_FALSE(rrs::password_verify("Secret123", ""));
    EXPECT_FALSE(rrs::password_verify("Secret123", "plaintext"));
    EXPECT_FALSE(rrs::password_verify("Secret123", "pbkdf2$100000$zz$zz"));
}

/* ---------- validation helpers ---------- */

TEST(ValidationTest, Emails) {
    EXPECT_TRUE(rrs::is_valid_email("a@b.com"));
    EXPECT_FALSE(rrs::is_valid_email(""));
    EXPECT_FALSE(rrs::is_valid_email("no-at-sign.com"));
    EXPECT_FALSE(rrs::is_valid_email("@b.com"));
    EXPECT_FALSE(rrs::is_valid_email("a@b"));
    EXPECT_FALSE(rrs::is_valid_email("a@.com"));
    EXPECT_FALSE(rrs::is_valid_email("a@b."));
    EXPECT_FALSE(rrs::is_valid_email("a b@c.com"));
    EXPECT_FALSE(rrs::is_valid_email("a@@b.com"));
    EXPECT_FALSE(rrs::is_valid_email(std::string(95, 'a') + "@b.com"));  /* > 100 */
}

TEST(ValidationTest, PasswordComplexityBoundaries) {
    EXPECT_TRUE(rrs::is_valid_password("Abcdef12"));                 /* exactly 8 */
    EXPECT_FALSE(rrs::is_valid_password("Abcde12"));                 /* 7 */
    EXPECT_FALSE(rrs::is_valid_password("abcdefg1"));                /* no upper */
    EXPECT_FALSE(rrs::is_valid_password("ABCDEFG1"));                /* no lower */
    EXPECT_FALSE(rrs::is_valid_password("Abcdefgh"));                /* no digit */
    EXPECT_TRUE(rrs::is_valid_password("Aa1" + std::string(61, 'x')));   /* 64 */
    EXPECT_FALSE(rrs::is_valid_password("Aa1" + std::string(62, 'x')));  /* 65 */
}

TEST(ValidationTest, Names) {
    EXPECT_TRUE(rrs::is_valid_name("Ridhima Jain"));
    EXPECT_FALSE(rrs::is_valid_name(""));
    EXPECT_FALSE(rrs::is_valid_name("   "));
    EXPECT_FALSE(rrs::is_valid_name("Bad\nName"));
    EXPECT_FALSE(rrs::is_valid_name(std::string(101, 'a')));
}

/* ---------- FR-001: registration ---------- */

TEST_F(AuthTest, RegisterValidPassengerSucceeds) {
    EXPECT_EQ(RRS_OK, auth_register("ridhima@example.com", "Passw0rdOK", "Ridhima Jain"));
}

TEST_F(AuthTest, RegisterRejectsNullInputs) {
    EXPECT_EQ(ERR_INVALID_INPUT, auth_register(nullptr, "Passw0rdOK", "A"));
    EXPECT_EQ(ERR_INVALID_INPUT, auth_register("a@b.com", nullptr, "A"));
    EXPECT_EQ(ERR_INVALID_INPUT, auth_register("a@b.com", "Passw0rdOK", nullptr));
}

TEST_F(AuthTest, RegisterRejectsBadEmail) {
    EXPECT_EQ(ERR_INVALID_INPUT, auth_register("not-an-email", "Passw0rdOK", "A"));
}

TEST_F(AuthTest, RegisterRejectsWeakPassword) {
    EXPECT_EQ(ERR_INVALID_INPUT, auth_register("a@b.com", "weak", "A"));
}

TEST_F(AuthTest, RegisterRejectsEmptyName) {
    EXPECT_EQ(ERR_INVALID_INPUT, auth_register("a@b.com", "Passw0rdOK", ""));
}

TEST_F(AuthTest, StoredRecordHoldsHashNotPlaintext) {      /* SEC-001 / ST-04 */
    ASSERT_EQ(RRS_OK, auth_register("a@b.com", "Passw0rdOK", "A"));
    std::string stored;
    ASSERT_TRUE(auth_get_stored_hash_for_tests("a@b.com", stored));
    EXPECT_EQ(stored.find("Passw0rdOK"), std::string::npos);
    EXPECT_EQ(0u, stored.rfind("pbkdf2$", 0));
}

TEST_F(AuthTest, TwoUsersSamePasswordHaveDifferentHashes) {   /* ST-04: salts differ */
    ASSERT_EQ(RRS_OK, auth_register("a@b.com", "Passw0rdOK", "A"));
    ASSERT_EQ(RRS_OK, auth_register("c@d.com", "Passw0rdOK", "C"));
    std::string h1, h2;
    ASSERT_TRUE(auth_get_stored_hash_for_tests("a@b.com", h1));
    ASSERT_TRUE(auth_get_stored_hash_for_tests("c@d.com", h2));
    EXPECT_NE(h1, h2);
}

/* ---------- FR-002: duplicate rejection ---------- */

TEST_F(AuthTest, DuplicateEmailIsRejected) {
    ASSERT_EQ(RRS_OK, auth_register("a@b.com", "Passw0rdOK", "A"));
    EXPECT_EQ(ERR_DUPLICATE, auth_register("a@b.com", "Another1Pass", "Other"));
}

TEST_F(AuthTest, DuplicateCheckIsCaseInsensitive) {
    ASSERT_EQ(RRS_OK, auth_register("User@Example.com", "Passw0rdOK", "A"));
    EXPECT_EQ(ERR_DUPLICATE, auth_register("user@example.COM", "Passw0rdOK", "B"));
}

TEST_F(AuthTest, FailedRegistrationDoesNotConsumeEmail) {
    EXPECT_EQ(ERR_INVALID_INPUT, auth_register("a@b.com", "weak", "A"));
    EXPECT_EQ(RRS_OK, auth_register("a@b.com", "Passw0rdOK", "A"));
}

/* ---------- FR-003: login and session with role ---------- */

class LoginTest : public ::testing::Test {
protected:
    void SetUp() override {
        auth_reset_for_tests();
        ASSERT_EQ(RRS_OK, auth_register("rider@example.com", "Passw0rdOK", "Rider"));
    }
};

TEST_F(LoginTest, ValidLoginReturnsPassengerSession) {
    rrs_session_t s;
    ASSERT_EQ(RRS_OK, auth_login("rider@example.com", "Passw0rdOK", &s));
    EXPECT_EQ(RRS_ROLE_PASSENGER, s.role);
    EXPECT_EQ(36u, std::strlen(s.user_id));
    EXPECT_EQ(64u, std::strlen(s.token));
}

TEST_F(LoginTest, SessionExpiryIsInTheFutureByTheTtl) {
    rrs_session_t s;
    time_t before = time(nullptr);
    ASSERT_EQ(RRS_OK, auth_login("rider@example.com", "Passw0rdOK", &s));
    EXPECT_GE(s.expiry, before + rrs::kSessionTtlSeconds);
    EXPECT_LE(s.expiry, time(nullptr) + rrs::kSessionTtlSeconds);
}

TEST_F(LoginTest, WrongPasswordFailsAndLeavesSessionEmpty) {
    rrs_session_t s;
    std::memset(&s, 0xAB, sizeof s);
    EXPECT_EQ(ERR_AUTH_FAILED, auth_login("rider@example.com", "WrongPass1", &s));
    EXPECT_EQ(0u, std::strlen(s.token));
    EXPECT_EQ(0u, std::strlen(s.user_id));
    EXPECT_EQ(0u, rrs::session_count());
}

TEST_F(LoginTest, UnknownEmailGivesSameErrorAsWrongPassword) {
    rrs_session_t s;
    EXPECT_EQ(ERR_AUTH_FAILED, auth_login("nobody@example.com", "Passw0rdOK", &s));
}

TEST_F(LoginTest, EmailMatchIsCaseInsensitive) {
    rrs_session_t s;
    EXPECT_EQ(RRS_OK, auth_login("RIDER@Example.COM", "Passw0rdOK", &s));
}

TEST_F(LoginTest, PasswordMatchIsCaseSensitive) {
    rrs_session_t s;
    EXPECT_EQ(ERR_AUTH_FAILED, auth_login("rider@example.com", "passw0rdok", &s));
}

TEST_F(LoginTest, NullArgumentsAreRejected) {
    rrs_session_t s;
    EXPECT_EQ(ERR_INVALID_INPUT, auth_login(nullptr, "Passw0rdOK", &s));
    EXPECT_EQ(ERR_INVALID_INPUT, auth_login("rider@example.com", nullptr, &s));
    EXPECT_EQ(ERR_INVALID_INPUT, auth_login("rider@example.com", "Passw0rdOK", nullptr));
}

TEST_F(LoginTest, EmptyCredentialsFailAuthentication) {
    rrs_session_t s;
    EXPECT_EQ(ERR_AUTH_FAILED, auth_login("", "", &s));
}

TEST_F(LoginTest, TwoLoginsGiveDifferentTokensForTheSameUser) {
    rrs_session_t a, b;
    ASSERT_EQ(RRS_OK, auth_login("rider@example.com", "Passw0rdOK", &a));
    ASSERT_EQ(RRS_OK, auth_login("rider@example.com", "Passw0rdOK", &b));
    EXPECT_STRNE(a.token, b.token);
    EXPECT_STREQ(a.user_id, b.user_id);
    EXPECT_EQ(2u, rrs::session_count());
}

TEST_F(LoginTest, DifferentUsersGetDifferentUserIds) {
    ASSERT_EQ(RRS_OK, auth_register("other@example.com", "Passw0rdOK", "Other"));
    rrs_session_t a, b;
    ASSERT_EQ(RRS_OK, auth_login("rider@example.com", "Passw0rdOK", &a));
    ASSERT_EQ(RRS_OK, auth_login("other@example.com", "Passw0rdOK", &b));
    EXPECT_STRNE(a.user_id, b.user_id);
}

TEST_F(LoginTest, AdminAccountGetsAdminRole) {
    ASSERT_EQ(RRS_OK, auth_create_user_for_tests("admin@example.com", "Adm1nPassw0rd",
                                                  "Admin", RRS_ROLE_ADMIN));
    rrs_session_t s;
    ASSERT_EQ(RRS_OK, auth_login("admin@example.com", "Adm1nPassw0rd", &s));
    EXPECT_EQ(RRS_ROLE_ADMIN, s.role);
}

TEST_F(LoginTest, SelfRegistrationNeverProducesAnAdmin) {
    rrs_session_t s;
    ASSERT_EQ(RRS_OK, auth_login("rider@example.com", "Passw0rdOK", &s));
    EXPECT_NE(RRS_ROLE_ADMIN, s.role);
}
