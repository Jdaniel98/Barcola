#pragma once

#include <cstddef>
#include <vector>

#include "core/price_history.h"

namespace barcola {

// Simple moving average, aligned to the input: result[i] is the mean of
// values[i - window + 1 .. i]. The first window - 1 entries are NaN because there is not
// enough data yet (NaN compares false, so crossover checks skip them naturally).
// Throws std::invalid_argument if window is 0.
[[nodiscard]] std::vector<double> calculateSMA(const std::vector<double>& values, size_t window);

// Exponential moving average with alpha = 2 / (window + 1), seeded with the SMA of the first
// window values. Aligned like calculateSMA, with the first window - 1 entries NaN.
// Throws std::invalid_argument if window is 0.
[[nodiscard]] std::vector<double> calculateEMA(const std::vector<double>& values, size_t window);

// Streaming moving average over the most recent `windowSize` values.
class MovingAverage {
public:
    // Throws std::invalid_argument if windowSize is 0.
    explicit MovingAverage(size_t windowSize);

    void addData(double value);
    [[nodiscard]] double getSMA() const;

    // EMA of the values currently in the window only, seeded with the oldest of them.
    // This is not the same as calculateEMA over a whole series.
    // Throws std::invalid_argument unless 0 < smoothingFactor <= 1.
    [[nodiscard]] double getEMA(double smoothingFactor) const;

private:
    size_t windowSize_;
    std::vector<double> data_;
};

// Prints SMA/EMA for each data point in the history
void printMovingAverages(const PriceHistory& history, size_t windowSize, double emaSmoothingFactor);

}  // namespace barcola
