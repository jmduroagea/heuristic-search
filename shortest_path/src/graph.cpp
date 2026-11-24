#include "graph.hpp"

#include <fstream>
#include <iostream>
#include <new>

namespace {

// DIMACS line prefixes.
constexpr char PROBLEM_LINE = 'p';
constexpr char ARC_LINE = 'a';
constexpr char VERTEX_LINE = 'v';

// Reads a whole file into memory with a single read: far fewer system calls
// than line-by-line reading on multi-gigabyte DIMACS files.
bool readWholeFile(const std::string& filename, std::vector<char>& buffer) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);

    try {
        buffer.resize(static_cast<size_t>(size));
    } catch (const std::bad_alloc&) {
        std::cerr << "Error: not enough memory to load " << filename << std::endl;
        return false;
    }
    return static_cast<bool>(file.read(buffer.data(), size));
}

// Minimal number parsers: skip to the next number and read it, without the
// locale handling and error checking of atoi or sscanf.
uint32_t parseUnsigned(const char*& ptr, const char* end) {
    while (ptr < end && (*ptr < '0' || *ptr > '9')) {
        ++ptr;
    }
    uint32_t value = 0;
    while (ptr < end && *ptr >= '0' && *ptr <= '9') {
        value = value * 10 + static_cast<uint32_t>(*ptr - '0');
        ++ptr;
    }
    return value;
}

int32_t parseSigned(const char*& ptr, const char* end) {
    while (ptr < end && *ptr != '-' && (*ptr < '0' || *ptr > '9')) {
        ++ptr;
    }
    const bool negative = (ptr < end && *ptr == '-');
    if (negative) {
        ++ptr;
    }
    const int32_t magnitude = static_cast<int32_t>(parseUnsigned(ptr, end));
    return negative ? -magnitude : magnitude;
}

void skipLine(const char*& ptr, const char* end) {
    while (ptr < end && *ptr != '\n') {
        ++ptr;
    }
    if (ptr < end) {
        ++ptr;
    }
}

}  // namespace

bool Graph::loadGraph(const std::string& filename) {
    std::vector<char> buffer;
    if (!readWholeFile(filename, buffer)) {
        std::cerr << "Error: cannot read " << filename << std::endl;
        return false;
    }

    const char* ptr = buffer.data();
    const char* const end = ptr + buffer.size();
    numEdges_ = 0;

    while (ptr < end) {
        if (*ptr == PROBLEM_LINE) {  // "p sp <nodes> <edges>"
            const uint32_t nodes = parseUnsigned(ptr, end);
            forward_.assign(nodes, std::vector<Edge>());
            reverse_.assign(nodes, std::vector<Edge>());
        } else if (*ptr == ARC_LINE) {  // "a <from> <to> <cost>", 1-based ids
            const uint32_t from = parseUnsigned(ptr, end) - 1;
            const uint32_t to = parseUnsigned(ptr, end) - 1;
            const uint32_t cost = parseUnsigned(ptr, end);
            if (from < numNodes() && to < numNodes()) {
                forward_[from].push_back({to, cost});
                reverse_[to].push_back({from, cost});
                ++numEdges_;
            }
        }
        skipLine(ptr, end);
    }

    if (numNodes() == 0) {
        std::cerr << "Error: " << filename << " has no valid problem line." << std::endl;
        return false;
    }
    return true;
}

bool Graph::loadCoordinates(const std::string& filename) {
    if (numNodes() == 0) {
        std::cerr << "Error: load the graph before its coordinates." << std::endl;
        return false;
    }

    std::vector<char> buffer;
    if (!readWholeFile(filename, buffer)) {
        std::cerr << "Error: cannot read " << filename << std::endl;
        return false;
    }

    const char* ptr = buffer.data();
    const char* const end = ptr + buffer.size();
    coordinates_.assign(numNodes(), Point{0, 0});
    uint32_t loaded = 0;

    while (ptr < end) {
        if (*ptr == VERTEX_LINE) {  // "v <id> <lon> <lat>", 1-based id
            const uint32_t id = parseUnsigned(ptr, end) - 1;
            const int32_t lon = parseSigned(ptr, end);
            const int32_t lat = parseSigned(ptr, end);
            if (id < numNodes()) {
                coordinates_[id] = {lat, lon};
                ++loaded;
            }
        }
        skipLine(ptr, end);
    }

    if (loaded != numNodes()) {
        std::cerr << "Error: " << filename << " has coordinates for " << loaded
                  << " of " << numNodes() << " nodes." << std::endl;
        return false;
    }
    return true;
}