#pragma once

#include <vector>

#include "core/price_history.h"

namespace barcola {

class MarketDataAnalysis {
public:
    explicit MarketDataAnalysis(const PriceHistory& priceHistory);

    [[nodiscard]] double calculateAveragePrice() const;

    // Annualized volatility: sample standard deviation of daily log returns x sqrt(252).
    // Returns 0.0 with fewer than two returns. Throws std::invalid_argument on a
    // non-positive price.
    [[nodiscard]] double calculateVolatility() const;

    [[nodiscard]] std::vector<PricePoint> getUpwardTrends() const;
    [[nodiscard]] std::vector<PricePoint> getDownwardTrends() const;

private:
    const PriceHistory& priceHistory_;
};

}  // namespace barcola
