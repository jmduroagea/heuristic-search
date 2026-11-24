#include "open_list.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

OpenList::OpenList() {
    heap_.reserve(INITIAL_CAPACITY);
}

void OpenList::push(uint32_t node, double priority) {
    heap_.push_back({node, priority});
    siftUp(heap_.size() - 1);
}

HeapEntry OpenList::pop() {
    assert(!heap_.empty());
    const HeapEntry best = heap_.front();
    heap_.front() = heap_.back();
    heap_.pop_back();
    if (!heap_.empty()) {
        siftDown(0);
    }
    return best;
}

double OpenList::minPriority() const {
    assert(!heap_.empty());
    return heap_.front().priority;
}

void OpenList::siftUp(size_t index) {
    while (index > 0) {
        const size_t parent = (index - 1) / ARITY;
        if (heap_[index].priority >= heap_[parent].priority) {
            break;
        }
        std::swap(heap_[index], heap_[parent]);
        index = parent;
    }
}

void OpenList::siftDown(size_t index) {
    const size_t size = heap_.size();
    while (true) {
        const size_t firstChild = ARITY * index + 1;
        if (firstChild >= size) {
            break;
        }
        const size_t childrenEnd = std::min(size, firstChild + ARITY);

        size_t smallest = index;
        for (size_t child = firstChild; child < childrenEnd; ++child) {
            if (heap_[child].priority < heap_[smallest].priority) {
                smallest = child;
            }
        }
        if (smallest == index) {
            break;
        }
        std::swap(heap_[index], heap_[smallest]);
        index = smallest;
    }
}