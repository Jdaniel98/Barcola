#include "indicators/rsi.h"

#include <cmath>

namespace barcola {

double calculateRSI(const std::vector<double>& prices, size_t period) {
    if (prices.size() <= period) {
        return 0.0;  // Insufficient data
    }

    double avgGain = 0.0;
    double avgLoss = 0.0;

    for (size_t i = 1; i <= period; ++i) {
        const double diff = prices[i] - prices[i - 1];
        if (diff > 0.0) {
            avgGain += diff;
        } else {
            avgLoss -= diff;  // Make positive
        }
    }

    avgGain /= static_cast<double>(period);
    avgLoss /= static_cast<double>(period);

    // FIX Bug 1: Guard against division by zero when avgLoss is zero (all gains)
    if (avgLoss < 1e-10) {
        return 100.0;
    }

    // Also handle edge case where avgGain is zero (all losses)
    if (avgGain < 1e-10) {
        return 0.0;
    }

    const double relativeStrength = avgGain / avgLoss;
    return 100.0 - (100.0 / (1.0 + relativeStrength));
}

}  // namespace barcola
