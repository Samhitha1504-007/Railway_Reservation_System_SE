/*
 * SEC-003: input validation. Test Plan 5.1: "fuzz all input fields with
 * over-long strings, special characters, negative numbers".
 * Covers every auth entry point that takes user text: auth_register,
 * auth_login and auth_profile_update.
 */
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "audit.h"
#include "audit_internal.h"
#include "auth.h"
#include "auth_internal.h"
#include "session.h"

namespace {

const char* kGoodPw = "Passw0rdOK";

/* tiny deterministic generator so failures are reproducible */
struct Rng {
    unsigned long long state;
    explicit Rng(unsigned long long seed) : state(seed) {}
    unsigned next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<unsigned>(state >> 33);
    }
};

/* never contains NUL (C strings cannot), otherwise any byte 1..255 */
std::string random_bytes(Rng& r, size_t len) {
    std::string s;
    for (size_t i = 0; i < len; ++i) s.push_back(static_cast<char>(1 + r.next() % 255));
    return s;
}

std::string random_from(Rng& r, size_t len, const std::string& alphabet) {
    std::string s;
    for (size_t i = 0; i < len; ++i) s.push_back(alphabet[r.next() % alphabet.size()]);
    return s;
}

bool status_is_one_of(rrs_status_t st, std::initializer_list<rrs_status_t> allowed) {
    for (rrs_status_t a : allowed) if (st == a) return true;
    return false;
}

class InputFuzzTest : public ::testing::Test {
protected:
    rrs_session_t s;
    void SetUp() override {
        auth_reset_for_tests();
        audit_reset_for_tests();
        ASSERT_EQ(RRS_OK, auth_register("rider@example.com", kGoodPw, "Rider One"));
        ASSERT_EQ(RRS_OK, auth_login("rider@example.com", kGoodPw, &s));
    }
    void TearDown() override { audit_reset_for_tests(); }

    std::string profile_name() {
        rrs_profile_t p;
        EXPECT_EQ(RRS_OK, auth_profile_get(&s, &p));
        return p.name;
    }
    std::string profile_email() {
        rrs_profile_t p;
        EXPECT_EQ(RRS_OK, auth_profile_get(&s, &p));
        return p.email;
    }
};

}  // namespace

/* ---------- over-long strings ---------- */

TEST_F(InputFuzzTest, OverLongFieldsAreRejectedByRegister) {
    for (size_t n : {101u, 1000u, 100000u}) {
        SCOPED_TRACE(n);
        std::string big(n, 'a');
        EXPECT_EQ(ERR_INVALID_INPUT, auth_register((big + "@example.com").c_str(), kGoodPw, "N"));
        EXPECT_EQ(ERR_INVALID_INPUT, auth_register("new@example.com", kGoodPw, big.c_str()));
        EXPECT_EQ(ERR_INVALID_INPUT, auth_register("new@example.com", ("Aa1" + big).c_str(), "N"));
    }
}

TEST_F(InputFuzzTest, OverLongFieldsFailCleanlyAtLogin) {
    rrs_session_t out;
    std::string big(100000, 'a');
    EXPECT_EQ(ERR_AUTH_FAILED, auth_login(big.c_str(), kGoodPw, &out));
    EXPECT_EQ(ERR_AUTH_FAILED, auth_login("rider@example.com", big.c_str(), &out));
    EXPECT_EQ(0u, std::strlen(out.token));
}

TEST_F(InputFuzzTest, OverLongFieldsAreRejectedByProfileUpdateAndChangeNothing) {
    std::string big(100000, 'a');
    EXPECT_EQ(ERR_INVALID_INPUT, auth_profile_update(&s, big.c_str(), nullptr));
    EXPECT_EQ(ERR_INVALID_INPUT, auth_profile_update(&s, nullptr, (big + "@example.com").c_str()));
    EXPECT_EQ("Rider One", profile_name());
    EXPECT_EQ("rider@example.com", profile_email());
}

TEST_F(InputFuzzTest, LengthBoundariesAreExact) {
    std::string e100 = std::string(94, 'a') + "@b.com";       /* 100 characters */
    EXPECT_EQ(100u, e100.size());
    EXPECT_TRUE(rrs::is_valid_email(e100));
    EXPECT_FALSE(rrs::is_valid_email("a" + e100));             /* 101 */
    EXPECT_TRUE(rrs::is_valid_name(std::string(100, 'n')));
    EXPECT_FALSE(rrs::is_valid_name(std::string(101, 'n')));
    EXPECT_TRUE(rrs::is_valid_password("Aa1" + std::string(61, 'x')));   /* 64 */
    EXPECT_FALSE(rrs::is_valid_password("Aa1" + std::string(62, 'x')));  /* 65 */
}

/* ---------- special characters ---------- */

TEST_F(InputFuzzTest, EmailsWithSpecialCharactersAreRejected) {
    const std::vector<std::string> bad = {
        "a\x01@b.com", "a\x1b[31m@b.com", "a\x7f@b.com", "a\tb@c.com", "a\nb@c.com",
        "a\rb@c.com", "a b@c.com", "a|b@c.com", "a\"b@c.com", "a'b@c.com", "a\\b@c.com",
        "a<script>@b.com", "a;b@c.com", "a,b@c.com", "a(b)@c.com", "a@b.com\n",
        "a@b.com ", " a@b.com", "'; DROP TABLE users;--@b.com", "%s%n%x@b.com@c.com",
        "\xC3\xA9@b.com" /* é */, "a@b\xC3\xA9.com", "a@@b.com", "a@b@c.com",
        "@b.com", "a@", "a@b", "a@.com", "a@b.", "a@b..com", "a..b@c.com", ".a@b.com",
        "a.@b.com", "a@-b.com", "a@b-.com", "a@b_c.com", "a@b.c_m",
    };
    for (const std::string& e : bad) {
        SCOPED_TRACE(e);
        EXPECT_FALSE(rrs::is_valid_email(e));
        EXPECT_EQ(ERR_INVALID_INPUT, auth_register(e.c_str(), kGoodPw, "Name"));
        EXPECT_EQ(ERR_INVALID_INPUT, auth_profile_update(&s, nullptr, e.c_str()));
    }
    EXPECT_EQ("rider@example.com", profile_email());
}

TEST_F(InputFuzzTest, OrdinaryEmailsStillWork) {
    for (const char* e : {"a@b.com", "first.last@example.co.in", "user+tag@mail.example.org",
                          "x_y-z%1@sub.domain.com", "A@B.COM", "admin@rrs.in"})
        EXPECT_TRUE(rrs::is_valid_email(e)) << e;
}

TEST_F(InputFuzzTest, NamesWithControlCharactersAreRejected) {
    for (const char* n : {"\x1b[2J", "bell\x07", "two\nlines", "tab\there", "\x7f", "a\rb", "\x01"}) {
        SCOPED_TRACE(n);
        EXPECT_EQ(ERR_INVALID_INPUT, auth_register("new@example.com", kGoodPw, n));
        EXPECT_EQ(ERR_INVALID_INPUT, auth_profile_update(&s, n, nullptr));
    }
    EXPECT_EQ("Rider One", profile_name());
}

TEST_F(InputFuzzTest, NamesWithPunctuationAndSqlOrHtmlAreStoredAsPlainText) {
    for (const char* n : {"O'Brien", "<b>bold</b>", "'; DROP TABLE users;--", "a|b=c d",
                          "%s%s%s%n%x", "Ridhima \xF0\x9F\x98\x80"}) {
        SCOPED_TRACE(n);
        ASSERT_EQ(RRS_OK, auth_profile_update(&s, n, nullptr));
        EXPECT_EQ(std::string(n), profile_name());                 /* stored exactly, never interpreted */
    }
}

TEST_F(InputFuzzTest, EmptyAndBlankFieldsAreRejected) {
    for (const char* n : {"", " ", "   ", " \t"}) {
        SCOPED_TRACE(n);
        EXPECT_EQ(ERR_INVALID_INPUT, auth_profile_update(&s, n, nullptr));
    }
    EXPECT_EQ(ERR_INVALID_INPUT, auth_register("", kGoodPw, "N"));
    EXPECT_EQ(ERR_INVALID_INPUT, auth_register("new@example.com", "", "N"));
}

/* ---------- negative numbers and numeric-looking text ---------- */

TEST_F(InputFuzzTest, NegativeAndHugeNumbersAsTextNeverCrashOrSlipThrough) {
    for (const char* v : {"-1", "-0", "-99999999999999999999", "99999999999999999999999",
                          "NaN", "1e999", "-2147483649", "0x7fffffff"}) {
        SCOPED_TRACE(v);
        EXPECT_EQ(ERR_INVALID_INPUT, auth_register(v, kGoodPw, "N"));                 /* not an email */
        EXPECT_EQ(ERR_INVALID_INPUT, auth_register("new@example.com", v, "N"));       /* too weak     */
        EXPECT_EQ(ERR_INVALID_INPUT, auth_profile_update(&s, nullptr, v));
        rrs_session_t out;
        EXPECT_EQ(ERR_AUTH_FAILED, auth_login(v, v, &out));
        ASSERT_EQ(RRS_OK, auth_profile_update(&s, v, nullptr));                       /* plain text name */
        EXPECT_EQ(std::string(v), profile_name());
    }
}

/* ---------- random bytes (deterministic fuzz) ---------- */

TEST_F(InputFuzzTest, RandomBytesNeverCrashRegisterAndAreOnlyAcceptedWhenValid) {
    Rng r(12345);
    for (int i = 0; i < 3000; ++i) {
        std::string email = random_bytes(r, r.next() % 130);
        std::string name = random_bytes(r, r.next() % 130);
        rrs_status_t st = auth_register(email.c_str(), kGoodPw, name.c_str());
        ASSERT_TRUE(status_is_one_of(st, {RRS_OK, ERR_INVALID_INPUT, ERR_DUPLICATE})) << i;
        if (st == RRS_OK) {                                  /* accepted => both fields really are valid */
            EXPECT_TRUE(rrs::is_valid_email(email)) << i;
            EXPECT_TRUE(rrs::is_valid_name(name)) << i;
        }
    }
}

TEST_F(InputFuzzTest, RandomEmailLikeStringsAreAcceptedOnlyWhenTheValidatorSaysSo) {
    Rng r(777);
    const std::string alphabet = "abcXYZ019.-_%+@@ '\"|<>;,\\\t\n";
    int accepted = 0;
    for (int i = 0; i < 400; ++i) {
        std::string email = random_from(r, 1 + r.next() % 24, alphabet);
        bool valid = rrs::is_valid_email(email);
        rrs_status_t st = auth_register(email.c_str(), kGoodPw, "Fuzz");
        if (valid) {
            EXPECT_TRUE(status_is_one_of(st, {RRS_OK, ERR_DUPLICATE})) << email;
            if (st == RRS_OK) ++accepted;
        } else {
            EXPECT_EQ(ERR_INVALID_INPUT, st) << email;
        }
    }
    EXPECT_LT(accepted, 100);                                /* sanity: the fuzz is mostly invalid */
}

TEST_F(InputFuzzTest, RandomBytesNeverCrashLoginAndNeverSucceed) {
    Rng r(2024);
    for (int i = 0; i < 40; ++i) {                           /* each unknown login does a real hash check */
        std::string email = random_bytes(r, r.next() % 120);
        std::string pw = random_bytes(r, r.next() % 120);
        rrs_session_t out;
        rrs_status_t st = auth_login(email.c_str(), pw.c_str(), &out);
        EXPECT_EQ(ERR_AUTH_FAILED, st) << i;
        EXPECT_EQ(0u, std::strlen(out.token));
    }
}

TEST_F(InputFuzzTest, RandomBytesNeverCrashProfileUpdateAndRejectedValuesChangeNothing) {
    Rng r(99);
    for (int i = 0; i < 1500; ++i) {
        std::string name = random_bytes(r, r.next() % 130);
        std::string email = random_bytes(r, r.next() % 130);
        std::string name_before = profile_name(), email_before = profile_email();
        rrs_status_t st = auth_profile_update(&s, name.c_str(), email.c_str());
        ASSERT_TRUE(status_is_one_of(st, {RRS_OK, ERR_INVALID_INPUT, ERR_DUPLICATE})) << i;
        if (st != RRS_OK) {
            EXPECT_EQ(name_before, profile_name()) << i;
            EXPECT_EQ(email_before, profile_email()) << i;
        }
    }
    /* whatever happened, the stored profile is still valid */
    EXPECT_TRUE(rrs::is_valid_email(profile_email()));
    EXPECT_TRUE(rrs::is_valid_name(profile_name()));
}

TEST_F(InputFuzzTest, AuditLinesStayOnOneLineOfPlainAsciiUnderFuzz) {
    Rng r(31337);
    for (int i = 0; i < 30; ++i) {
        std::string email = random_bytes(r, 1 + r.next() % 150);
        rrs_session_t out;
        auth_login(email.c_str(), "Wr0ngPass1", &out);
    }
    ASSERT_GT(audit_entry_count_for_tests(), 0u);
    std::string line;
    for (size_t i = 0; audit_entry_for_tests(i, line); ++i) {
        EXPECT_LT(line.size(), 300u);
        for (unsigned char c : line) {
            EXPECT_GE(c, 0x20) << line;
            EXPECT_LT(c, 0x7f) << line;
        }
    }
}
