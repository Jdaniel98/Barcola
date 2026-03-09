#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace barcola {

// Calculate Bollinger Bands from closing prices.
// Returns {upper_bands, lower_bands}.
[[nodiscard]] std::pair<std::vector<double>, std::vector<double>> calculateBollingerBands(
    const std::vector<double>& prices, size_t period, double stdDevFactor);

}  // namespace barcola
