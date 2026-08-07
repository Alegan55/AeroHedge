#pragma once

#include <atomic>
#include <cstddef>
#include <new>

namespace aerohedge {

template <typename T, size_t Capacity>
class SPSCRingBuffer {
public:
    SPSCRingBuffer() = default;
    ~SPSCRingBuffer() = default;

    // Non-copyable and non-movable
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer(SPSCRingBuffer&&) = delete;
    SPSCRingBuffer& operator=(SPSCRingBuffer&&) = delete;

    // Producer thread calls this
    bool push(const T& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = increment(current_tail);

        // Check if buffer is full (head hasn't caught up)
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; 
        }

        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // Consumer thread calls this
    bool pop(T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);

        // Check if buffer is empty (head equals tail)
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; 
        }

        item = buffer_[current_head];
        head_.store(increment(current_head), std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

private:
    constexpr size_t increment(size_t idx) const noexcept {
        return (idx + 1) % Capacity;
    }

    // 64-byte alignment to prevent CPU cache-line bouncing (False Sharing)
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

    // Pre-allocated static array: zero heap allocations on the critical path
    T buffer_[Capacity];
};

} // namespace aerohedge
