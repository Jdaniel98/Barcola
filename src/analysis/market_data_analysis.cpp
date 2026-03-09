#include "analysis/market_data_analysis.h"

#include <cmath>

namespace barcola {

MarketDataAnalysis::MarketDataAnalysis(PriceHistory& priceHistory)
    : priceHistory_(priceHistory) {}

double MarketDataAnalysis::calculateAveragePrice() const {
    const size_t count = priceHistory_.dataPointsCount();
    if (count == 0) {
        return 0.0;
    }

    double total = 0.0;
    for (size_t i = 0; i < count; ++i) {
        total += priceHistory_.getDataPoint(i).getClosing();
    }
    return total / static_cast<double>(count);
}

double MarketDataAnalysis::calculateVolatility() const {
    const size_t count = priceHistory_.dataPointsCount();
    if (count < 2) {
        return 0.0;
    }

    const double averagePrice = calculateAveragePrice();
    double sumSquaredDifferences = 0.0;

    for (size_t i = 0; i < count; ++i) {
        const double price = priceHistory_.getDataPoint(i).getClosing();
        sumSquaredDifferences += (price - averagePrice) * (price - averagePrice);
    }

    return std::sqrt(sumSquaredDifferences / static_cast<double>(count - 1));
}

std::vector<PricePoint> MarketDataAnalysis::getUpwardTrends() const {
    std::vector<PricePoint> upwardTrends;
    const size_t count = priceHistory_.dataPointsCount();

    for (size_t i = 1; i < count; ++i) {
        const auto prevPoint = priceHistory_.getDataPoint(i - 1);
        const auto currentPoint = priceHistory_.getDataPoint(i);

        if (currentPoint.getClosing() > prevPoint.getClosing()) {
            upwardTrends.push_back(currentPoint);
        }
    }

    return upwardTrends;
}

std::vector<PricePoint> MarketDataAnalysis::getDownwardTrends() const {
    std::vector<PricePoint> downwardTrends;
    const size_t count = priceHistory_.dataPointsCount();

    for (size_t i = 1; i < count; ++i) {
        const auto prevPoint = priceHistory_.getDataPoint(i - 1);
        const auto currentPoint = priceHistory_.getDataPoint(i);

        if (currentPoint.getClosing() < prevPoint.getClosing()) {
            downwardTrends.push_back(currentPoint);
        }
    }

    return downwardTrends;
}

}  // namespace barcola
