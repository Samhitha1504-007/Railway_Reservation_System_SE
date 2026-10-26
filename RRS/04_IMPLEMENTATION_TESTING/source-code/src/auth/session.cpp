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
    time_t      expiry;     /* idle deadline; pushed forward by every successful call */
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

/* Find the live session that matches the caller's copy.
 * Returns its index, or -1 with `status` saying why. Sessions found to be idle
 * for too long are removed here. */
int locate(const rrs_session_t* s, SessionStatus& status) {
    status = SessionStatus::Invalid;
    if (!s) return -1;
    /* both strings must be NUL-terminated inside their buffers before we read them */
    if (!std::memchr(s->token, '\0', RRS_TOKEN_LEN)) return -1;
    if (!std::memchr(s->user_id, '\0', RRS_USER_ID_LEN)) return -1;

    const size_t want = RRS_TOKEN_LEN - 1;
    if (std::strlen(s->token) != want) return -1;

    for (size_t i = 0; i < g_sessions.size(); ++i) {
        const StoredSession& st = g_sessions[i];
        /* constant-time compare of the two equal-length tokens */
        if (st.token.size() != want || CRYPTO_memcmp(st.token.data(), s->token, want) != 0)
            continue;
        if (st.user_id != s->user_id) return -1;            /* tampered copy */
        if (now() >= st.expiry) {
            g_sessions.erase(g_sessions.begin() + static_cast<long>(i));
            status = SessionStatus::Expired;
            return -1;
        }
        status = SessionStatus::Valid;
        return static_cast<int>(i);
    }
    return -1;
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

SessionStatus session_authenticate(const rrs_session_t* session, SessionInfo& info) {
    SessionStatus status;
    int i = locate(session, status);
    if (i < 0) return status;

    StoredSession& st = g_sessions[static_cast<size_t>(i)];
    st.expiry = now() + kSessionTtlSeconds;               /* activity restarts the idle timer */
    info.user_id = st.user_id;
    info.role = st.role;
    info.expiry = st.expiry;
    return SessionStatus::Valid;
}

bool session_destroy(const rrs_session_t* session) {
    SessionStatus status;
    int i = locate(session, status);
    if (i < 0) return false;
    g_sessions.erase(g_sessions.begin() + i);
    return true;
}

size_t session_count() { return g_sessions.size(); }

void session_clear() { g_sessions.clear(); }

void session_set_fixed_time_for_tests(time_t t) { g_fixed_now = t; }

}  // namespace rrs
