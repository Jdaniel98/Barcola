#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/price_history.h"

namespace barcola {

class DynamicHedging {
public:
    DynamicHedging(const PriceHistory& priceHistory);

    void performDynamicHedging(size_t shortWindow = 20, size_t longWindow = 50) const;

    // Compute moving averages without printing (for testing)
    struct MAResult {
        std::vector<double> shortMA;
        std::vector<double> longMA;
    };
    [[nodiscard]] MAResult computeMovingAverages(size_t shortWindow, size_t longWindow) const;

private:
    const PriceHistory& priceHistory_;
};

}  // namespace barcola
