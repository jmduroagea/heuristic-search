#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <cstdint>
#include <string>
#include <vector>

// Coordinates in millionths of a degree, as stored in DIMACS .co files.
struct Point {
    int32_t lat;
    int32_t lon;
};

struct Edge {
    uint32_t to;
    uint32_t cost;
};

// Directed weighted graph stored as adjacency lists. The reverse graph is kept
// as well, so the backward search can walk edges against their direction.
// Node ids are 0-based internally; DIMACS files are 1-based.
class Graph {
public:
    bool loadGraph(const std::string& filename);
    bool loadCoordinates(const std::string& filename);  // call after loadGraph

    // No bounds checks: these sit on the hot path of the search, so callers
    // must pass ids below numNodes() (main validates user input).
    const std::vector<Edge>& outgoing(uint32_t node) const { return forward_[node]; }
    const std::vector<Edge>& incoming(uint32_t node) const { return reverse_[node]; }
    const Point& point(uint32_t node) const { return coordinates_[node]; }

    uint32_t numNodes() const { return static_cast<uint32_t>(forward_.size()); }
    uint32_t numEdges() const { return numEdges_; }

private:
    std::vector<std::vector<Edge>> forward_;
    std::vector<std::vector<Edge>> reverse_;
    std::vector<Point> coordinates_;
    uint32_t numEdges_ = 0;
};

#endif  // GRAPH_HPP