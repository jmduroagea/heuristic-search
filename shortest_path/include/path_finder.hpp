#ifndef PATH_FINDER_HPP
#define PATH_FINDER_HPP

#include "closed_list.hpp"
#include "graph.hpp"

#include <cstdint>
#include <vector>

struct SearchResult {
    bool found = false;
    uint32_t cost = 0;
    uint64_t nodesExpanded = 0;
    double seconds = 0.0;
    std::vector<uint32_t> path;  // 0-based node ids, source first
};

class PathFinder {
public:
    explicit PathFinder(const Graph& graph);

    SearchResult bidirectionalAStar(uint32_t source, uint32_t target) const;
    SearchResult dijkstra(uint32_t source, uint32_t target) const;  // baseline

private:
    struct Vec3 {
        double x, y, z;
    };
    struct Query;        // source, target and the estimate between them
    struct Frontier;     // state of one search direction
    struct BestMeeting;  // best complete path found so far

    double chord(uint32_t a, uint32_t b) const;
    double calibrateScale() const;
    double estimate(uint32_t from, uint32_t to) const { return scale_ * chord(from, to); }
    double potential(uint32_t node, const Query& query, Direction direction) const;
    void expandNext(Frontier& self, const Frontier& other, ClosedList& closed,
                    const Query& query, BestMeeting& best, uint64_t& expanded) const;

    const Graph& graph_;
    std::vector<Vec3> unitVectors_;  // node positions on the unit sphere
    double scale_;                   // cost units per unit of chord length
};

#endif  // PATH_FINDER_HPP