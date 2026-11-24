#include "graph.hpp"
#include "path_finder.hpp"

#include <algorithm>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

const std::string ASTAR = "astar";
const std::string DIJKSTRA = "dijkstra";

// Converts a 1-based DIMACS node id from the command line into a 0-based index.
bool parseNodeId(const std::string& text, uint32_t numNodes, uint32_t& index) {
    try {
        size_t parsedChars = 0;
        const unsigned long id = std::stoul(text, &parsedChars);
        if (parsedChars != text.size() || id == 0 || id > numNodes) {
            return false;
        }
        index = static_cast<uint32_t>(id - 1);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Cost of the cheapest edge u -> v (road graphs can contain parallel edges).
uint32_t edgeCost(const Graph& graph, uint32_t u, uint32_t v) {
    uint32_t cheapest = std::numeric_limits<uint32_t>::max();
    for (const Edge& edge : graph.outgoing(u)) {
        if (edge.to == v) {
            cheapest = std::min(cheapest, edge.cost);
        }
    }
    return cheapest;
}

// Writes the path as "1 - (1498) - 308 - (8718) - 309": 1-based node ids,
// with the cost of each edge between them in parentheses.
bool writeSolution(const std::string& filename, const std::vector<uint32_t>& path,
                   const Graph& graph) {
    std::ofstream out(filename);
    if (!out) {
        return false;
    }
    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
            out << " - (" << edgeCost(graph, path[i - 1], path[i]) << ") - ";
        }
        out << path[i] + 1;
    }
    out << '\n';
    return static_cast<bool>(out);
}

void printUsage() {
    std::cerr << "Usage: main_solver <graph.gr> <coords.co> <source> <target> "
              << "<output> <" << ASTAR << "|" << DIJKSTRA << ">" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 7) {
        printUsage();
        return 1;
    }
    const std::string graphFile = argv[1];
    const std::string coordsFile = argv[2];
    const std::string outputFile = argv[5];
    const std::string algorithm = argv[6];

    if (algorithm != ASTAR && algorithm != DIJKSTRA) {
        std::cerr << "Error: unknown algorithm '" << algorithm << "'." << std::endl;
        printUsage();
        return 1;
    }

    Graph graph;
    std::cout << "Loading " << graphFile << " and " << coordsFile << "..." << std::endl;
    if (!graph.loadGraph(graphFile) || !graph.loadCoordinates(coordsFile)) {
        return 1;
    }
    std::cout << "# vertices: " << graph.numNodes() << std::endl;
    std::cout << "# edges: " << graph.numEdges() << std::endl;

    uint32_t source = 0;
    uint32_t target = 0;
    if (!parseNodeId(argv[3], graph.numNodes(), source) ||
        !parseNodeId(argv[4], graph.numNodes(), target)) {
        std::cerr << "Error: node ids must be integers between 1 and "
                  << graph.numNodes() << "." << std::endl;
        return 1;
    }

    const PathFinder finder(graph);
    const SearchResult result = (algorithm == DIJKSTRA)
                                    ? finder.dijkstra(source, target)
                                    : finder.bidirectionalAStar(source, target);

    if (result.found) {
        std::cout << "Optimal path cost: " << result.cost << std::endl;
    } else {
        std::cout << "No path found." << std::endl;
    }
    const double nodesPerSecond =
        (result.seconds > 0.0) ? result.nodesExpanded / result.seconds : 0.0;
    std::cout << std::fixed << std::setprecision(2)
              << "Search time: " << result.seconds << " s" << std::endl
              << "# expansions: " << result.nodesExpanded << " (" << nodesPerSecond
              << " nodes/s)" << std::endl;

    if (result.found && !writeSolution(outputFile, result.path, graph)) {
        std::cerr << "Error: cannot write " << outputFile << std::endl;
        return 1;
    }
    return 0;
}