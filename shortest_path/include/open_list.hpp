#ifndef OPEN_LIST_HPP
#define OPEN_LIST_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

struct HeapEntry {
    uint32_t node;
    double priority;  // g + potential for A* (can be fractional), g for Dijkstra
};

// Open list implemented as a 4-ary min-heap over a flat vector. The four
// children of a node sit next to each other in memory (usually in one cache
// line), and the tree is half as tall as a binary heap.
//
// Uses lazy deletion: when a node's priority improves, it is pushed again
// instead of updated in place, and callers skip the stale entries on pop.
class OpenList {
public:
    OpenList();

    void push(uint32_t node, double priority);
    HeapEntry pop();             // precondition: !empty()
    double minPriority() const;  // precondition: !empty()

    bool empty() const { return heap_.empty(); }
    size_t size() const { return heap_.size(); }

private:
    static constexpr size_t ARITY = 4;
    static constexpr size_t INITIAL_CAPACITY = 10000;

    void siftUp(size_t index);
    void siftDown(size_t index);

    std::vector<HeapEntry> heap_;
};

#endif  // OPEN_LIST_HPP