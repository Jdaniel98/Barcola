#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/price_history.h"

namespace barcola {

constexpr size_t CROSSOVER_SHORT_WINDOW = 20;
constexpr size_t CROSSOVER_LONG_WINDOW = 50;
// One bar more than the long window, so the first crossover can be detected.
constexpr size_t MONTE_CARLO_MIN_BARS = CROSSOVER_LONG_WINDOW + 1;

struct MonteCarloResult {
    std::vector<double> profits;  // one per simulation, sorted ascending
    double meanProfit = 0.0;
    double medianProfit = 0.0;
    double percentile5 = 0.0;
    double percentile95 = 0.0;
};

// Profit of a dual SMA(20/50) crossover rule on one price path. On a golden cross (short
// average crosses above long) it invests 20% of cash; on a death cross it sells everything.
// Shares still held are valued at the last price. Returns 0.0 with fewer than
// MONTE_CARLO_MIN_BARS prices.
[[nodiscard]] double runSmaCrossover(const std::vector<double>& closes, double initialInvestment);

// Each simulation builds a synthetic price path as long as `history` by resampling its
// daily log returns with replacement (a bootstrap), starting from the first close, and runs
// runSmaCrossover on that path. The same seed always gives the same result.
// Returns an empty result with fewer than MONTE_CARLO_MIN_BARS bars.
// Throws std::invalid_argument if numSimulations is 0, initialInvestment <= 0, or a price
// is not positive.
[[nodiscard]] MonteCarloResult performMonteCarloSimulation(const PriceHistory& history,
                                                           size_t numSimulations,
                                                           double initialInvestment,
                                                           std::uint32_t seed = 42);

}  // namespace barcola
