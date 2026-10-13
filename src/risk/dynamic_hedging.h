#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/price_history.h"

namespace barcola {

// Short and long simple moving averages of the closing price, the inputs a moving-average
// hedging rule would use. No hedge ratio or hedge trades are computed yet.
class DynamicHedging {
public:
    DynamicHedging(const PriceHistory& priceHistory);

    // Prints both averages for every bar where both are defined.
    // Throws std::invalid_argument unless 0 < shortWindow < longWindow.
    void performDynamicHedging(size_t shortWindow = 20, size_t longWindow = 50) const;

    // shortMA[k] covers closes [k, k + shortWindow); longMA[k] covers [k, k + longWindow).
    // Both are empty if there are fewer than longWindow bars.
    struct MAResult {
        std::vector<double> shortMA;
        std::vector<double> longMA;
    };
    // Throws std::invalid_argument unless 0 < shortWindow < longWindow.
    [[nodiscard]] MAResult computeMovingAverages(size_t shortWindow, size_t longWindow) const;

private:
    const PriceHistory& priceHistory_;
};

}  // namespace barcola
