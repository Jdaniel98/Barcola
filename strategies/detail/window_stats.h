#pragma once

// Internal, not public API: the window arithmetic behind the strategies' decisions. It lives
// here so the strategies, the decision-margin report and apps/decision_values all run the same
// expressions.

#include <cmath>
#include <cstddef>

#include "backtest/strategy.h"

namespace barcola::detail {

// A standard deviation below this counts as no variance at all
inline constexpr double FLAT_STD_DEV = 1e-12;

// Mean of the last `window` closes; the caller guarantees bars.size() >= window.
[[nodiscard]] inline double trailingMean(const BarView& bars, size_t window) {
    double sum = 0.0;
    for (size_t i = bars.size() - window; i < bars.size(); ++i) {
        sum += bars[i].getClosing();
    }
    return sum / static_cast<double>(window);
}

// z-score of the latest close against the mean and standard deviation of the last `lookback`
// closes, the latest one included; the caller guarantees bars.size() >= lookback. Returns 0.0
// when the closes have no variance.
[[nodiscard]] inline double zScore(const BarView& bars, size_t lookback) {
    const size_t end = bars.size();
    const size_t start = end - lookback;

    double sum = 0.0;
    for (size_t i = start; i < end; ++i) {
        sum += bars[i].getClosing();
    }
    const double mean = sum / static_cast<double>(lookback);

    double sumSqDiff = 0.0;
    for (size_t i = start; i < end; ++i) {
        const double diff = bars[i].getClosing() - mean;
        sumSqDiff += diff * diff;
    }
    const double stdDev = std::sqrt(sumSqDiff / static_cast<double>(lookback));

    if (stdDev < FLAT_STD_DEV) {
        return 0.0;  // No variance — price is flat
    }

    return (bars.back().getClosing() - mean) / stdDev;
}

// How far value sits from base, as a share of base. Returns 0.0 when base is zero, where the
// gap is undefined; prices are never zero, and a margin of 0.0 can only overstate the risk.
[[nodiscard]] inline double relativeGap(double value, double base) {
    if (base == 0.0) {
        return 0.0;
    }
    return (value - base) / base;
}

}  // namespace barcola::detail
