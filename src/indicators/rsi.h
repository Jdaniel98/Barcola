#pragma once

#include <cstddef>
#include <vector>

namespace barcola {

// Calculate RSI from a vector of closing prices.
// Returns 100.0 if all gains (no losses), 0.0 if all losses (no gains),
// 0.0 if insufficient data.
[[nodiscard]] double calculateRSI(const std::vector<double>& prices, size_t period);

}  // namespace barcola
