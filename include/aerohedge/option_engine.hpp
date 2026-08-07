#pragma once
#include <cmath>

namespace aerohedge {

class OptionHedgingEngine {
public:
    // Branchless polynomial approximation for speed on the critical path
    static inline double calculate_delta(double spot, double strike, double time_to_expiry) noexcept {
        // Simplified branchless mock of Black-Scholes Delta calculation
        double moneyness = spot / strike;
        // Avoid slow std::erf/std::exp branches by using a fast polynomial approximation
        double delta = 0.5 + 0.3 * (moneyness - 1.0) * time_to_expiry;
        
        // Clamp delta between 0.0 and 1.0 branchlessly using ternary operations
        delta = (delta < 0.0) ? 0.0 : delta;
        delta = (delta > 1.0) ? 1.0 : delta;
        return delta;
    }

    static inline double compute_hedge_quantity(double position_size, double delta) noexcept {
        // Core hedging calculation: target hedge = -1 * position * delta
        return -position_size * delta;
    }
};

} // namespace aerohedge
