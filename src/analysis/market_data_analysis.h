#pragma once

#include <vector>

#include "core/price_history.h"

namespace barcola {

class MarketDataAnalysis {
public:
    explicit MarketDataAnalysis(PriceHistory& priceHistory);

    [[nodiscard]] double calculateAveragePrice() const;
    [[nodiscard]] double calculateVolatility() const;
    [[nodiscard]] std::vector<PricePoint> getUpwardTrends() const;
    [[nodiscard]] std::vector<PricePoint> getDownwardTrends() const;

private:
    PriceHistory& priceHistory_;
};

}  // namespace barcola
