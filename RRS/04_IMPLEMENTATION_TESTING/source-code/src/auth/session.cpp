#include "session.h"

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
    s.expiry = time(nullptr) + kSessionTtlSeconds;

    std::memset(&out, 0, sizeof out);
    std::strncpy(out.user_id, s.user_id.c_str(), RRS_USER_ID_LEN - 1);
    std::strncpy(out.token, s.token.c_str(), RRS_TOKEN_LEN - 1);
    out.role = s.role;
    out.expiry = s.expiry;

    g_sessions.push_back(s);
    return true;
}

size_t session_count() { return g_sessions.size(); }

void session_clear() { g_sessions.clear(); }

}  // namespace rrs
