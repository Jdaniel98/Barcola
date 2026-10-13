#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace barcola {

// Calculate Bollinger Bands from closing prices, using the population standard deviation.
// Returns {upper_bands, lower_bands}, one entry per full window: entry k covers
// prices[k .. k + period - 1]. Empty if there are fewer than `period` prices.
// Throws std::invalid_argument if period is 0.
[[nodiscard]] std::pair<std::vector<double>, std::vector<double>> calculateBollingerBands(
    const std::vector<double>& prices, size_t period, double stdDevFactor);

}  // namespace barcola
