#include "indicators/rsi.h"

#include <algorithm>
#include <stdexcept>

namespace barcola {

namespace {

constexpr double RSI_NEUTRAL = 50.0;
constexpr double RSI_MAX = 100.0;

}  // namespace

std::optional<double> calculateRSI(const std::vector<double>& prices, size_t period) {
    if (period == 0) {
        throw std::invalid_argument("RSI period must be at least 1");
    }
    if (prices.size() <= period) {
        return std::nullopt;
    }

    const double periodD = static_cast<double>(period);

    // Seed: simple average of the first `period` gains and losses
    double avgGain = 0.0;
    double avgLoss = 0.0;
    for (size_t i = 1; i <= period; ++i) {
        const double diff = prices[i] - prices[i - 1];
        avgGain += std::max(diff, 0.0);
        avgLoss += std::max(-diff, 0.0);
    }
    avgGain /= periodD;
    avgLoss /= periodD;

    // FIX: the original stopped here, so the result only ever reflected the first `period`
    // changes. Wilder smoothing carries the averages through to the latest price.
    for (size_t i = period + 1; i < prices.size(); ++i) {
        const double diff = prices[i] - prices[i - 1];
        avgGain = (avgGain * (periodD - 1.0) + std::max(diff, 0.0)) / periodD;
        avgLoss = (avgLoss * (periodD - 1.0) + std::max(-diff, 0.0)) / periodD;
    }

    if (avgLoss == 0.0) {
        return avgGain == 0.0 ? RSI_NEUTRAL : RSI_MAX;
    }

    const double relativeStrength = avgGain / avgLoss;
    return RSI_MAX - (RSI_MAX / (1.0 + relativeStrength));
}

}  // namespace barcola
