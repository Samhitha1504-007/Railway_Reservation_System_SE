/*
 * search.cpp - direct (FR-012) and connected multi-leg (FR-013) route search.
 * Depth-limited search over the route graph: from each station, ride any
 * train to a later halt, then change trains there if more legs are allowed.
 */
#include "search.h"
#include "graph.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

const int kMinutesPerDay = 24 * 60;

struct LegRef {
    size_t train;
    size_t from_halt;
    size_t to_halt;
    int depart;   /* minutes after 00:00 on the travel date */
    int arrive;
};

struct Candidate {
    std::vector<LegRef> legs;
    int total_minutes() const { return legs.back().arrive - legs.front().depart; }
};

struct Search {
    std::string dst;
    int max_legs;
    int min_transfer;
    std::set<std::string> visited;   /* stations on the current path */
    std::vector<LegRef> path;
    std::vector<Candidate> found;

    void from(const std::string& station, int ready_at, size_t prev_train, bool first);
};

/* `ready_at` is when we arrived at `station` (ignored for the first leg). */
void Search::from(const std::string& station, int ready_at, size_t prev_train, bool first) {
    const std::vector<rrs::StopRef>* stops = rrs::graph_stops_at(station);
    if (!stops) return;
    const std::vector<rrs::Train>& trains = rrs::graph_trains();

    for (const rrs::StopRef& stop : *stops) {
        if (!first && stop.train == prev_train) continue;   /* same train is not a change */
        const rrs::Train& t = trains[stop.train];
        const rrs::Halt& board = t.halts[stop.halt];

        /* trains run daily: take the first run that leaves after we are ready */
        int depart;
        if (first) {
            depart = board.depart_min % kMinutesPerDay;
        } else {
            int wait = (board.depart_min % kMinutesPerDay - ready_at % kMinutesPerDay + kMinutesPerDay)
                       % kMinutesPerDay;
            while (wait < min_transfer) wait += kMinutesPerDay;
            depart = ready_at + wait;
        }

        for (size_t h = stop.halt + 1; h < t.halts.size(); ++h) {
            const rrs::Halt& alight = t.halts[h];
            if (visited.count(alight.station)) continue;
            int arrive = depart + (alight.arrive_min - board.depart_min);
            path.push_back(LegRef{stop.train, stop.halt, h, depart, arrive});

            if (alight.station == dst) {
                found.push_back(Candidate{path});
                path.pop_back();
                break;   /* no point riding past the destination */
            }
            if (static_cast<int>(path.size()) < max_legs) {
                visited.insert(alight.station);
                from(alight.station, arrive, stop.train, false);
                visited.erase(alight.station);
            }
            path.pop_back();
        }
    }
}

bool is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

bool is_valid_date(const char* s) {
    if (std::strlen(s) != 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
        if (s[i] < '0' || s[i] > '9') return false;
    int y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    int m = (s[5] - '0') * 10 + (s[6] - '0');
    int d = (s[8] - '0') * 10 + (s[9] - '0');
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (y < 1 || m < 1 || m > 12 || d < 1) return false;
    int max_day = days[m - 1] + (m == 2 && is_leap(y) ? 1 : 0);
    return d <= max_day;
}

bool ranks_before(const Candidate& a, const Candidate& b) {
    if (a.total_minutes() != b.total_minutes()) return a.total_minutes() < b.total_minutes();
    if (a.legs.size() != b.legs.size()) return a.legs.size() < b.legs.size();
    return a.legs.front().depart < b.legs.front().depart;
}

void copy_code(char* dst, size_t cap, const std::string& src) {
    std::snprintf(dst, cap, "%s", src.c_str());
}

void fill_route(const Candidate& c, rrs_route_t& out) {
    const std::vector<rrs::Train>& trains = rrs::graph_trains();
    std::memset(&out, 0, sizeof out);
    out.leg_count = static_cast<int>(c.legs.size());
    for (size_t i = 0; i < c.legs.size(); ++i) {
        const LegRef& l = c.legs[i];
        const rrs::Train& t = trains[l.train];
        rrs_leg_t& leg = out.legs[i];
        copy_code(leg.train_id, sizeof leg.train_id, t.id);
        copy_code(leg.from, sizeof leg.from, t.halts[l.from_halt].station);
        copy_code(leg.to, sizeof leg.to, t.halts[l.to_halt].station);
        leg.depart_min = l.depart;
        leg.arrive_min = l.arrive;
        leg.distance_km = t.halts[l.to_halt].distance_km - t.halts[l.from_halt].distance_km;
        out.total_distance_km += leg.distance_km;
    }
    out.total_minutes = c.total_minutes();
}

}  // namespace

extern "C" rrs_search_opts_t search_default_opts(void) {
    rrs_search_opts_t o;
    o.max_legs = 2;
    o.max_results = RRS_MAX_ROUTES;
    o.min_transfer_minutes = 30;
    return o;
}

extern "C" rrs_status_t search_routes(const char* src, const char* dst, const char* date,
                                      const rrs_search_opts_t* opts, rrs_route_list_t* out) {
    if (!out) return ERR_INVALID_INPUT;
    out->count = 0;
    if (!src || !dst || !date) return ERR_INVALID_INPUT;

    rrs_search_opts_t o = opts ? *opts : search_default_opts();
    if (o.max_legs < 1 || o.max_legs > RRS_MAX_LEGS) return ERR_INVALID_INPUT;
    if (o.max_results < 1 || o.max_results > RRS_MAX_ROUTES) return ERR_INVALID_INPUT;
    if (o.min_transfer_minutes < 0 || o.min_transfer_minutes >= kMinutesPerDay) return ERR_INVALID_INPUT;

    std::string from(src), to(dst);
    if (!rrs::is_valid_station_code(from) || !rrs::is_valid_station_code(to)) return ERR_INVALID_INPUT;
    if (from == to || !is_valid_date(date)) return ERR_INVALID_INPUT;
    if (!rrs::graph_stops_at(from) || !rrs::graph_stops_at(to)) return ERR_INVALID_INPUT;

    Search s;
    s.dst = to;
    s.max_legs = o.max_legs;
    s.min_transfer = o.min_transfer_minutes;
    s.visited.insert(from);
    s.from(from, 0, 0, true);
    if (s.found.empty()) return ERR_NO_ROUTE;

    /* The same trains can connect at several stations: keep the fastest. */
    std::map<std::vector<size_t>, Candidate> best;
    for (const Candidate& c : s.found) {
        std::vector<size_t> key;
        for (const LegRef& l : c.legs) key.push_back(l.train);
        auto it = best.find(key);
        if (it == best.end() || ranks_before(c, it->second)) best[key] = c;
    }

    /* FR-013: connections only when no direct train, or when they arrive sooner. */
    int fastest_direct = -1;
    for (const auto& kv : best)
        if (kv.second.legs.size() == 1 &&
            (fastest_direct < 0 || kv.second.total_minutes() < fastest_direct))
            fastest_direct = kv.second.total_minutes();

    std::vector<Candidate> ranked;
    for (const auto& kv : best) {
        const Candidate& c = kv.second;
        if (c.legs.size() == 1 || fastest_direct < 0 || c.total_minutes() < fastest_direct)
            ranked.push_back(c);
    }
    std::stable_sort(ranked.begin(), ranked.end(), ranks_before);

    int n = std::min(static_cast<int>(ranked.size()), o.max_results);
    for (int i = 0; i < n; ++i) fill_route(ranked[i], out->routes[i]);
    out->count = n;
    return RRS_OK;
}
