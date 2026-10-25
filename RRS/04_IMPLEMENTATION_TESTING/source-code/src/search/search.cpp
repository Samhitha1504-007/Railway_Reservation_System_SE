/*
 * search.cpp - direct (FR-012) and connected multi-leg (FR-013) route search.
 * Depth-limited search over the route graph: from each station, ride any
 * train to a later halt, then change trains there if more legs are allowed.
 *
 * Pruning keeps 3-leg searches fast on a 500-train network:
 *  - only change at a station that can still reach the destination with the
 *    trains left (reverse breadth-first search from the destination);
 *  - drop a connection once it is already slower than the fastest direct train;
 *  - drop a partial journey once it is already slower than the max_results-th
 *    best journey found so far (it can only get slower).
 */
#include "search.h"
#include "graph.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
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

bool ranks_before(const Candidate& a, const Candidate& b) {
    if (a.total_minutes() != b.total_minutes()) return a.total_minutes() < b.total_minutes();
    if (a.legs.size() != b.legs.size()) return a.legs.size() < b.legs.size();
    return a.legs.front().depart < b.legs.front().depart;
}

/* Fewest trains needed to reach `dst` from each station, up to `max_trains`.
 * Stations not in the map need more than that. */
std::unordered_map<std::string, int> trains_to_reach(const std::string& dst, int max_trains) {
    const std::vector<rrs::Train>& trains = rrs::graph_trains();
    std::unordered_map<std::string, int> dist;
    dist[dst] = 0;
    for (int level = 1; level <= max_trains; ++level) {
        std::vector<std::string> added;
        for (const rrs::Train& t : trains) {
            /* walking the route backwards: is any later halt at level - 1? */
            bool reaches = false;
            for (size_t h = t.halts.size(); h-- > 0;) {
                const std::string& st = t.halts[h].station;
                auto it = dist.find(st);
                if (reaches && it == dist.end()) added.push_back(st);
                if (it != dist.end() && it->second == level - 1) reaches = true;
            }
        }
        if (added.empty()) break;
        for (const std::string& st : added) dist.emplace(st, level);
    }
    return dist;
}

struct Search {
    std::string dst;
    int max_legs;
    int max_results;
    int min_transfer;
    int fastest_direct;                            /* INT_MAX if none */
    std::unordered_map<std::string, int> dist;     /* trains still needed */
    std::set<std::string> visited;                 /* stations on the current path */
    std::vector<LegRef> path;

    /* best journey per train sequence (the same trains can connect at several
     * stations), plus their totals for the max_results-th best bound */
    std::map<std::vector<size_t>, Candidate> best;
    std::multiset<int> totals;

    void record(const Candidate& c);
    int bound() const;
    bool uses_train(size_t train) const;
    void from(const std::string& station, int ready_at, bool first);
};

void Search::record(const Candidate& c) {
    std::vector<size_t> key;
    for (const LegRef& l : c.legs) key.push_back(l.train);
    auto it = best.find(key);
    if (it != best.end()) {
        if (!ranks_before(c, it->second)) return;
        totals.erase(totals.find(it->second.total_minutes()));
        it->second = c;
    } else {
        best.emplace(key, c);
    }
    totals.insert(c.total_minutes());
}

/* A journey slower than this cannot make the results. */
int Search::bound() const {
    int b = fastest_direct;
    if (static_cast<int>(totals.size()) >= max_results) {
        auto it = totals.begin();
        std::advance(it, max_results - 1);
        b = std::min(b, *it);
    }
    return b;
}

bool Search::uses_train(size_t train) const {
    for (const LegRef& l : path)
        if (l.train == train) return true;
    return false;
}

/* `ready_at` is when we arrived at `station` (ignored for the first leg). */
void Search::from(const std::string& station, int ready_at, bool first) {
    const std::vector<rrs::StopRef>* stops = rrs::graph_stops_at(station);
    if (!stops) return;
    const std::vector<rrs::Train>& trains = rrs::graph_trains();
    int legs_after = static_cast<int>(path.size()) + 1;   /* legs once we board */

    for (const rrs::StopRef& stop : *stops) {
        if (uses_train(stop.train)) continue;
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
        int start = first ? depart : path.front().depart;

        for (size_t h = stop.halt + 1; h < t.halts.size(); ++h) {
            const rrs::Halt& alight = t.halts[h];
            int arrive = depart + (alight.arrive_min - board.depart_min);
            if (arrive - start > bound()) break;   /* only gets slower */
            if (visited.count(alight.station)) continue;

            if (alight.station == dst) {
                path.push_back(LegRef{stop.train, stop.halt, h, depart, arrive});
                Candidate c{path};
                path.pop_back();
                if (c.legs.size() == 1 || c.total_minutes() < fastest_direct) record(c);
                break;   /* no point riding past the destination */
            }

            auto d = dist.find(alight.station);
            if (d == dist.end() || legs_after + d->second > max_legs) continue;

            path.push_back(LegRef{stop.train, stop.halt, h, depart, arrive});
            visited.insert(alight.station);
            from(alight.station, arrive, false);
            visited.erase(alight.station);
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
    s.max_legs = 1;
    s.max_results = o.max_results;
    s.min_transfer = o.min_transfer_minutes;
    s.fastest_direct = INT_MAX;
    s.dist[to] = 0;
    s.visited.insert(from);

    /* direct trains first, so connections can be pruned against the fastest */
    s.from(from, 0, true);
    for (const auto& kv : s.best)
        s.fastest_direct = std::min(s.fastest_direct, kv.second.total_minutes());

    if (o.max_legs > 1) {
        s.max_legs = o.max_legs;
        s.dist = trains_to_reach(to, o.max_legs - 1);
        s.from(from, 0, true);
    }
    if (s.best.empty()) return ERR_NO_ROUTE;

    std::vector<Candidate> ranked;
    for (const auto& kv : s.best) ranked.push_back(kv.second);
    std::stable_sort(ranked.begin(), ranked.end(), ranks_before);

    int n = std::min(static_cast<int>(ranked.size()), o.max_results);
    for (int i = 0; i < n; ++i) fill_route(ranked[i], out->routes[i]);
    out->count = n;
    return RRS_OK;
}
