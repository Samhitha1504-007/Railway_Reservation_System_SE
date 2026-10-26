#include "auth.h"
#include "auth_internal.h"
#include "password.h"
#include "session.h"

#include <openssl/rand.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct UserRecord {
    std::string user_id;
    std::string email;          /* normalized (lower-case) */
    std::string name;
    rrs_role_t  role;
    std::string password_hash;  /* encoded: pbkdf2$iter$salt$hash - never plaintext */
};

/* In-memory store for now. File persistence is a later task. */
std::vector<UserRecord> g_users;

bool generate_user_id(std::string& out) {
    unsigned char b[16];
    if (RAND_bytes(b, sizeof b) != 1) return false;
    static const char* hex = "0123456789abcdef";
    out.clear();
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out.push_back('-');
        out.push_back(hex[b[i] >> 4]);
        out.push_back(hex[b[i] & 0x0F]);
    }
    return true;   /* 36 chars, UUID-shaped */
}

const UserRecord* find_user(const std::string& normalized_email) {
    for (const auto& u : g_users)
        if (u.email == normalized_email) return &u;
    return nullptr;
}

}  // namespace

namespace rrs {

std::string normalize_email(const std::string& email) {
    std::string out = email;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool is_valid_email(const std::string& email) {
    if (email.empty() || email.size() > 100) return false;
    if (email.find_first_of(" \t\r\n") != std::string::npos) return false;
    size_t at = email.find('@');
    if (at == std::string::npos || at == 0) return false;
    if (email.find('@', at + 1) != std::string::npos) return false;
    std::string domain = email.substr(at + 1);
    size_t dot = domain.find('.');
    if (dot == std::string::npos || dot == 0 || domain.back() == '.') return false;
    return true;
}

bool is_valid_password(const std::string& pw) {
    if (pw.size() < 8 || pw.size() > 64) return false;
    bool upper = false, lower = false, digit = false;
    for (unsigned char c : pw) {
        if (std::isupper(c)) upper = true;
        else if (std::islower(c)) lower = true;
        else if (std::isdigit(c)) digit = true;
    }
    return upper && lower && digit;
}

bool is_valid_name(const std::string& name) {
    if (name.empty() || name.size() > 100) return false;
    bool has_visible = false;
    for (unsigned char c : name) {
        if (std::iscntrl(c)) return false;
        if (!std::isspace(c)) has_visible = true;
    }
    return has_visible;
}

}  // namespace rrs

namespace {

rrs_status_t create_user(const char* email, const char* password, const char* name,
                         rrs_role_t role) {
    if (!email || !password || !name) return ERR_INVALID_INPUT;

    std::string e(email), p(password), n(name);
    if (!rrs::is_valid_email(e) || !rrs::is_valid_password(p) || !rrs::is_valid_name(n))
        return ERR_INVALID_INPUT;

    std::string key = rrs::normalize_email(e);
    if (find_user(key)) return ERR_DUPLICATE;

    UserRecord rec;
    rec.email = key;
    rec.name = n;
    rec.role = role;
    if (!generate_user_id(rec.user_id)) return ERR_IO;
    if (!rrs::password_hash(p, rec.password_hash)) return ERR_IO;

    g_users.push_back(rec);
    return RRS_OK;
}

}  // namespace

extern "C" rrs_status_t auth_register(const char* email, const char* password, const char* name) {
    /* self-registration is always a passenger */
    return create_user(email, password, name, RRS_ROLE_PASSENGER);
}

extern "C" rrs_status_t auth_login(const char* email, const char* password, rrs_session_t* out) {
    if (!out) return ERR_INVALID_INPUT;
    std::memset(out, 0, sizeof *out);
    if (!email || !password) return ERR_INVALID_INPUT;

    const UserRecord* u = find_user(rrs::normalize_email(email));

    if (!u) {
        /* Unknown email: still do a hash check against a dummy so the response
         * time does not reveal whether the email exists. */
        static std::string dummy;
        if (dummy.empty()) rrs::password_hash("Dummy-Password-1", dummy);
        rrs::password_verify(password, dummy);
        return ERR_AUTH_FAILED;
    }

    if (!rrs::password_verify(password, u->password_hash)) return ERR_AUTH_FAILED;

    if (!rrs::session_create(u->user_id, u->role, *out)) {
        std::memset(out, 0, sizeof *out);
        return ERR_IO;
    }
    return RRS_OK;
}

extern "C" rrs_status_t auth_logout(const rrs_session_t* session) {
    return rrs::session_destroy(session) ? RRS_OK : ERR_SESSION_EXPIRED;
}

void auth_reset_for_tests(void) {
    g_users.clear();
    rrs::session_clear();
}

rrs_status_t auth_create_user_for_tests(const char* email, const char* password,
                                        const char* name, rrs_role_t role) {
    return create_user(email, password, name, role);
}

bool auth_get_stored_hash_for_tests(const char* email, std::string& hash_out) {
    const UserRecord* u = find_user(rrs::normalize_email(email));
    if (!u) return false;
    hash_out = u->password_hash;
    return true;
}
