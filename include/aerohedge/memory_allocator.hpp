#pragma once
#include <atomic>
#include <cstddef>

namespace aerohedge {

class MemoryEnforcer {
public:
    static void lock_allocations() noexcept {
        allocations_locked_.store(true, std::memory_order_release);
    }

    static bool is_locked() noexcept {
        return allocations_locked_.load(std::memory_order_acquire);
    }

private:
    static inline std::atomic<bool> allocations_locked_{false};
};

} // namespace aerohedge
