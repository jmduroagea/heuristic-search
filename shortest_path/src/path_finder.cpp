#include "path_finder.hpp"

#include "open_list.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double MICRODEGREES_PER_DEGREE = 1e6;  // DIMACS .co format
constexpr double TO_RAD = PI / (180.0 * MICRODEGREES_PER_DEGREE);

constexpr uint32_t INF = std::numeric_limits<uint32_t>::max();
constexpr uint32_t NO_PARENT = std::numeric_limits<uint32_t>::max();

using Clock = std::chrono::steady_clock;

double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

// Follows parent links from `node` back to the origin of that search.
// Returns the nodes in that order: `node` first, origin last.
std::vector<uint32_t> traceToOrigin(uint32_t node, const std::vector<uint32_t>& parent) {
    std::vector<uint32_t> path;
    for (uint32_t current = node; current != NO_PARENT; current = parent[current]) {
        path.push_back(current);
    }
    return path;
}

}  // namespace

struct PathFinder::Query {
    uint32_t source;
    uint32_t target;
    double sourceToTarget;  // estimate(source, target)
};

struct PathFinder::Frontier {
    Frontier(size_t numNodes, uint32_t origin, Direction direction)
        : g(numNodes, INF), parent(numNodes, NO_PARENT), direction(direction) {
        g[origin] = 0;
    }

    OpenList open;
    std::vector<uint32_t> g;       // best known cost from this search's origin
    std::vector<uint32_t> parent;  // previous node on that best path
    Direction direction;
};

struct PathFinder::BestMeeting {
    uint32_t cost = INF;
    uint32_t node = NO_PARENT;

    void offer(uint32_t pathCost, uint32_t meetingNode) {
        if (pathCost < cost) {
            cost = pathCost;
            node = meetingNode;
        }
    }
};

PathFinder::PathFinder(const Graph& graph) : graph_(graph), unitVectors_(graph.numNodes()) {
    for (uint32_t node = 0; node < graph_.numNodes(); ++node) {
        const double lat = graph_.point(node).lat * TO_RAD;
        const double lon = graph_.point(node).lon * TO_RAD;
        unitVectors_[node] = {std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon),
                              std::sin(lat)};
    }
    scale_ = calibrateScale();
}

// Straight-line (chord) distance between two nodes through the unit sphere.
// It is a true distance, so it obeys the triangle inequality, it is never
// longer than the path along the surface, and it needs no trigonometry.
double PathFinder::chord(uint32_t a, uint32_t b) const {
    const Vec3& p = unitVectors_[a];
    const Vec3& q = unitVectors_[b];
    const double dx = p.x - q.x;
    const double dy = p.y - q.y;
    const double dz = p.z - q.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Largest scale such that scale * chord(u, v) <= cost(u, v) on every edge.
// With that scale, the estimate can never exceed the real cost of any path
// (triangle inequality), whatever unit the costs use: decimetres in the DIMACS
// distance graphs, but this would also work for travel times.
double PathFinder::calibrateScale() const {
    double scale = std::numeric_limits<double>::infinity();
    for (uint32_t node = 0; node < graph_.numNodes(); ++node) {
        for (const Edge& edge : graph_.outgoing(node)) {
            const double length = chord(node, edge.to);
            if (length > 0.0) {
                scale = std::min(scale, edge.cost / length);
            }
        }
    }
    // No usable edge: a zero estimate turns the search into bidirectional Dijkstra.
    return std::isinf(scale) ? 0.0 : scale;
}

// Balanced potential (Ikeda et al., 1994): both searches use the average of
// "estimate to target" and "minus estimate to source", so they are guided
// towards each other and meet in the middle. The forward and backward
// potentials always add up to sourceToTarget, which is what makes the
// stopping rule in bidirectionalAStar() valid. The sourceToTarget / 2 offset
// only keeps the values non-negative; it does not change the search order.
double PathFinder::potential(uint32_t node, const Query& query, Direction direction) const {
    const double forward =
        0.5 * (estimate(node, query.target) - estimate(node, query.source) + query.sourceToTarget);
    return direction == Direction::Forward ? forward : query.sourceToTarget - forward;
}

// Pops one node from `self` and expands it. Whenever an edge improves a node
// that the other search has already reached, the two half paths form a
// complete path, which may improve `best`.
void PathFinder::expandNext(Frontier& self, const Frontier& other, ClosedList& closed,
                            const Query& query, BestMeeting& best, uint64_t& expanded) const {
    const uint32_t node = self.open.pop().node;
    if (closed.isClosed(node, self.direction)) {
        return;  // stale heap entry left by lazy deletion
    }
    closed.close(node, self.direction);
    ++expanded;

    const std::vector<Edge>& edges = (self.direction == Direction::Forward)
                                         ? graph_.outgoing(node)
                                         : graph_.incoming(node);
    for (const Edge& edge : edges) {
        const uint32_t newG = self.g[node] + edge.cost;
        if (newG < self.g[edge.to]) {
            self.g[edge.to] = newG;
            self.parent[edge.to] = node;
            self.open.push(edge.to, newG + potential(edge.to, query, self.direction));
            if (other.g[edge.to] != INF) {
                best.offer(newG + other.g[edge.to], edge.to);
            }
        }
    }
}

SearchResult PathFinder::bidirectionalAStar(uint32_t source, uint32_t target) const {
    const Clock::time_point start = Clock::now();
    const size_t numNodes = graph_.numNodes();
    const Query query{source, target, estimate(source, target)};

    Frontier forward(numNodes, source, Direction::Forward);
    Frontier backward(numNodes, target, Direction::Backward);
    forward.open.push(source, potential(source, query, Direction::Forward));
    backward.open.push(target, potential(target, query, Direction::Backward));

    ClosedList closed(numNodes);
    BestMeeting best;
    if (source == target) {
        best.offer(0, source);
    }
    uint64_t expanded = 0;

    while (!forward.open.empty() && !backward.open.empty()) {
        // Stopping rule for balanced potentials: any path not found yet would
        // cost at least minPriority(forward) + minPriority(backward) - sourceToTarget.
        if (forward.open.minPriority() + backward.open.minPriority() >=
            best.cost + query.sourceToTarget) {
            break;
        }
        // Cardinality criterion: grow the smaller frontier.
        if (forward.open.size() <= backward.open.size()) {
            expandNext(forward, backward, closed, query, best, expanded);
        } else {
            expandNext(backward, forward, closed, query, best, expanded);
        }
    }

    SearchResult result;
    result.nodesExpanded = expanded;
    if (best.cost != INF) {
        result.found = true;
        result.cost = best.cost;

        // Source..meeting from the forward search, then meeting..target from
        // the backward one (skipping the meeting node, already included).
        result.path = traceToOrigin(best.node, forward.parent);
        std::reverse(result.path.begin(), result.path.end());
        const std::vector<uint32_t> toTarget = traceToOrigin(best.node, backward.parent);
        result.path.insert(result.path.end(), toTarget.begin() + 1, toTarget.end());
    }
    result.seconds = secondsSince(start);
    return result;
}

SearchResult PathFinder::dijkstra(uint32_t source, uint32_t target) const {
    const Clock::time_point start = Clock::now();
    const size_t numNodes = graph_.numNodes();

    std::vector<uint32_t> dist(numNodes, INF);
    std::vector<uint32_t> parent(numNodes, NO_PARENT);
    OpenList open;
    dist[source] = 0;
    open.push(source, 0);

    SearchResult result;
    while (!open.empty()) {
        const HeapEntry top = open.pop();
        const uint32_t node = top.node;

        if (top.priority > dist[node]) {
            continue;  // stale entry left by lazy deletion
        }
        if (node == target) {
            result.found = true;
            result.cost = dist[node];
            break;
        }
        ++result.nodesExpanded;

        for (const Edge& edge : graph_.outgoing(node)) {
            const uint32_t newDist = dist[node] + edge.cost;
            if (newDist < dist[edge.to]) {
                dist[edge.to] = newDist;
                parent[edge.to] = node;
                open.push(edge.to, newDist);
            }
        }
    }

    if (result.found) {
        result.path = traceToOrigin(target, parent);
        std::reverse(result.path.begin(), result.path.end());
    }
    result.seconds = secondsSince(start);
    return result;
}