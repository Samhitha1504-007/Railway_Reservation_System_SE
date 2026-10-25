#include "search.h"
#include "graph.h"

#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace {

std::vector<rrs::Train> g_trains;
std::unordered_map<std::string, std::vector<rrs::StopRef>> g_stops;   /* station -> stops */

bool is_valid_train_id(const std::string& id) {
    if (id.empty() || id.size() >= RRS_TRAIN_ID_LEN) return false;
    for (unsigned char c : id)
        if (!std::isalnum(c)) return false;
    return true;
}

}  // namespace

namespace rrs {

const std::vector<Train>& graph_trains() { return g_trains; }

const std::vector<StopRef>* graph_stops_at(const std::string& station) {
    auto it = g_stops.find(station);
    return it == g_stops.end() ? nullptr : &it->second;
}

bool is_valid_station_code(const std::string& code) {
    if (code.empty() || code.size() >= RRS_STATION_CODE_LEN) return false;
    for (unsigned char c : code)
        if (!std::isupper(c) && !std::isdigit(c)) return false;
    return true;
}

}  // namespace rrs

extern "C" rrs_status_t search_graph_add_train(const char* train_id, const rrs_halt_t* halts,
                                               int halt_count) {
    if (!train_id || !halts || halt_count < 2 || halt_count > RRS_MAX_HALTS)
        return ERR_INVALID_INPUT;

    rrs::Train t;
    t.id = train_id;
    if (!is_valid_train_id(t.id)) return ERR_INVALID_INPUT;

    std::unordered_set<std::string> seen;
    for (int i = 0; i < halt_count; ++i) {
        const rrs_halt_t& h = halts[i];
        /* the code must be NUL-terminated inside its buffer before we read it */
        size_t len = 0;
        while (len < RRS_STATION_CODE_LEN && h.station[len] != '\0') ++len;
        if (len == RRS_STATION_CODE_LEN) return ERR_INVALID_INPUT;

        rrs::Halt out{std::string(h.station, len), h.arrive_min, h.depart_min, h.distance_km};
        if (!rrs::is_valid_station_code(out.station)) return ERR_INVALID_INPUT;
        if (!seen.insert(out.station).second) return ERR_INVALID_INPUT;   /* repeated station */
        if (out.arrive_min < 0 || out.depart_min < out.arrive_min) return ERR_INVALID_INPUT;

        if (i == 0) {
            if (out.distance_km != 0 || out.depart_min >= 24 * 60) return ERR_INVALID_INPUT;
        } else {
            const rrs::Halt& prev = t.halts.back();
            if (out.arrive_min < prev.depart_min) return ERR_INVALID_INPUT;
            if (out.distance_km <= prev.distance_km) return ERR_INVALID_INPUT;
        }
        t.halts.push_back(out);
    }

    for (const auto& existing : g_trains)
        if (existing.id == t.id) return ERR_DUPLICATE;

    size_t index = g_trains.size();
    for (size_t h = 0; h < t.halts.size(); ++h)
        g_stops[t.halts[h].station].push_back(rrs::StopRef{index, h});
    g_trains.push_back(std::move(t));
    return RRS_OK;
}

extern "C" void search_graph_clear(void) {
    g_trains.clear();
    g_stops.clear();
}
