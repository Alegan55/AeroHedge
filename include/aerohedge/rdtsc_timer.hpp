#pragma once
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

namespace aerohedge {

class RDSTCTimer {
public:
    // Read the CPU Time-Stamp Counter directly (takes ~20 cycles)
    static inline uint64_t rdtsc() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        return __rdtsc();
#else
        return 0; // Fallback if not x86_64
#endif
    }
};

} // namespace aerohedge
