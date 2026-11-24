#ifndef CLOSED_LIST_HPP
#define CLOSED_LIST_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

enum class Direction { Forward, Backward };

// Closed list shared by both search directions, one byte per node:
// one bit means "closed by the forward search", another "closed by the
// backward search". A node with both bits set is where the searches met.
class ClosedList {
public:
    explicit ClosedList(size_t numNodes) : flags_(numNodes, 0) {}

    void close(uint32_t node, Direction direction) {
        flags_[node] = static_cast<uint8_t>(flags_[node] | mask(direction));
    }

    bool isClosed(uint32_t node, Direction direction) const {
        return (flags_[node] & mask(direction)) != 0;
    }

private:
    static constexpr uint8_t FORWARD_BIT = 1u << 0;
    static constexpr uint8_t BACKWARD_BIT = 1u << 1;

    static uint8_t mask(Direction direction) {
        return direction == Direction::Forward ? FORWARD_BIT : BACKWARD_BIT;
    }

    std::vector<uint8_t> flags_;
};

#endif  // CLOSED_LIST_HPP