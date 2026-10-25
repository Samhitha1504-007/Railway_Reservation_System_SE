#include "session.h"

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <cstring>
#include <ctime>
#include <vector>

namespace rrs {
namespace {

struct StoredSession {
    std::string token;
    std::string user_id;
    rrs_role_t  role;
    time_t      expiry;
};

std::vector<StoredSession> g_sessions;
time_t g_fixed_now = 0;   /* 0 = use the real clock (tests may freeze it) */

time_t now() { return g_fixed_now != 0 ? g_fixed_now : time(nullptr); }

bool random_token(std::string& out) {
    unsigned char b[32];                       /* 256 bits -> 64 hex chars */
    if (RAND_bytes(b, sizeof b) != 1) return false;
    static const char* hex = "0123456789abcdef";
    out.clear();
    for (unsigned char c : b) {
        out.push_back(hex[c >> 4]);
        out.push_back(hex[c & 0x0F]);
    }
    return true;
}

}  // namespace

bool session_create(const std::string& user_id, rrs_role_t role, rrs_session_t& out) {
    StoredSession s;
    if (!random_token(s.token)) return false;
    s.user_id = user_id;
    s.role = role;
    s.expiry = now() + kSessionTtlSeconds;

    std::memset(&out, 0, sizeof out);
    std::strncpy(out.user_id, s.user_id.c_str(), RRS_USER_ID_LEN - 1);
    std::strncpy(out.token, s.token.c_str(), RRS_TOKEN_LEN - 1);
    out.role = s.role;
    out.expiry = s.expiry;

    g_sessions.push_back(s);
    return true;
}

SessionStatus session_validate(const char* token, SessionInfo& info) {
    if (!token) return SessionStatus::Invalid;
    const size_t want = RRS_TOKEN_LEN - 1;
    if (std::strlen(token) != want) return SessionStatus::Invalid;

    for (const auto& s : g_sessions) {
        /* constant-time compare of the two equal-length tokens */
        if (s.token.size() == want && CRYPTO_memcmp(s.token.data(), token, want) == 0) {
            if (now() >= s.expiry) return SessionStatus::Expired;
            info.user_id = s.user_id;
            info.role = s.role;
            info.expiry = s.expiry;
            return SessionStatus::Valid;
        }
    }
    return SessionStatus::Invalid;
}

size_t session_count() { return g_sessions.size(); }

void session_clear() { g_sessions.clear(); }

void session_set_fixed_time_for_tests(time_t t) { g_fixed_now = t; }

}  // namespace rrs
