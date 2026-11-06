#include "audit.h"
#include "audit_internal.h"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <deque>
#include <string>

namespace {

std::deque<std::string> g_entries;     /* most recent lines, oldest first */
FILE* g_file = nullptr;
time_t g_fixed_now = 0;

time_t now() { return g_fixed_now != 0 ? g_fixed_now : time(nullptr); }

/* Keep only [A-Za-z0-9@._:-], cut to `max_len`. Empty becomes "-". */
std::string clean(const char* s, size_t max_len) {
    std::string out;
    if (s) {
        for (size_t i = 0; s[i] != '\0' && out.size() < max_len; ++i) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            bool ok = std::isalnum(c) || c == '@' || c == '.' || c == ':' || c == '-' || c == '_';
            out.push_back(ok ? static_cast<char>(c) : '_');
        }
    }
    return out.empty() ? "-" : out;
}

std::string timestamp() {
    time_t t = now();
    struct tm parts;
    gmtime_r(&t, &parts);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &parts);
    return buf;
}

}  // namespace

extern "C" const char* audit_status_name(rrs_status_t status) {
    switch (status) {
        case RRS_OK:                return "RRS_OK";
        case RRS_STATUS_ERROR:      return "RRS_STATUS_ERROR";
        case ERR_INVALID_INPUT:     return "ERR_INVALID_INPUT";
        case ERR_AUTH_FAILED:       return "ERR_AUTH_FAILED";
        case ERR_FORBIDDEN:         return "ERR_FORBIDDEN";
        case ERR_SESSION_EXPIRED:   return "ERR_SESSION_EXPIRED";
        case ERR_NOT_FOUND:         return "ERR_NOT_FOUND";
        case ERR_DUPLICATE:         return "ERR_DUPLICATE";
        case ERR_CAPACITY_CONFLICT: return "ERR_CAPACITY_CONFLICT";
        case ERR_PAYMENT_FAILED:    return "ERR_PAYMENT_FAILED";
        case ERR_NO_ROUTE:          return "ERR_NO_ROUTE";
        case ERR_IO:                return "ERR_IO";
    }
    return "ERR_UNKNOWN";
}

extern "C" rrs_status_t audit_init(const char* path) {
    audit_close();
    if (!path || path[0] == '\0') return RRS_OK;
    g_file = std::fopen(path, "a");
    return g_file ? RRS_OK : ERR_IO;
}

extern "C" void audit_close(void) {
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

extern "C" rrs_status_t audit_event(const char* event, const char* user, const char* function,
                                    rrs_status_t status) {
    std::string line = timestamp() + " event=" + clean(event, 32) + " user=" + clean(user, 100) +
                       " fn=" + clean(function, 64) + " status=" + audit_status_name(status);

    g_entries.push_back(line);
    while (g_entries.size() > kAuditMemoryEntries) g_entries.pop_front();

    if (!g_file) return RRS_OK;
    if (std::fprintf(g_file, "%s\n", line.c_str()) < 0 || std::fflush(g_file) != 0) return ERR_IO;
    return RRS_OK;
}

size_t audit_entry_count_for_tests(void) { return g_entries.size(); }

bool audit_entry_for_tests(size_t index, std::string& line_out) {
    if (index >= g_entries.size()) return false;
    line_out = g_entries[index];
    return true;
}

void audit_reset_for_tests(void) {
    g_entries.clear();
    audit_close();
    g_fixed_now = 0;
}

void audit_set_fixed_time_for_tests(time_t t) { g_fixed_now = t; }
