/*
 * graph.h - route graph used by the search module (internal).
 * Adjacency list: each station maps to the (train, halt) stops made there.
 * Travelling from a stop to any later halt of the same train is an edge.
 */
#ifndef RRS_SEARCH_GRAPH_H
#define RRS_SEARCH_GRAPH_H

#include <cstddef>
#include <string>
#include <vector>

namespace rrs {

struct Halt {
    std::string station;
    int arrive_min;
    int depart_min;
    int distance_km;
};

struct Train {
    std::string id;
    std::vector<Halt> halts;
};

/* A train stopping at a station: index into graph_trains() and into its halts. */
struct StopRef {
    size_t train;
    size_t halt;
};

const std::vector<Train>& graph_trains();

/* Stops made at `station`, or nullptr if no train serves it. */
const std::vector<StopRef>* graph_stops_at(const std::string& station);

/* Station code: 1-5 characters, upper-case letters or digits. */
bool is_valid_station_code(const std::string& code);

}  // namespace rrs

#endif
