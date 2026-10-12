#include "analysis/market_data_analysis.h"

#include <cmath>
#include <stdexcept>

namespace barcola {

namespace {

constexpr double TRADING_DAYS = 252.0;

}  // namespace

MarketDataAnalysis::MarketDataAnalysis(const PriceHistory& priceHistory)
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

// FIX: this used to be the standard deviation of price levels, which grows with the price
// itself and with any trend. Volatility is measured on returns.
double MarketDataAnalysis::calculateVolatility() const {
    const std::vector<double> closes = priceHistory_.getClosingPrices();
    if (closes.size() < 3) {
        return 0.0;
    }

    std::vector<double> returns;
    returns.reserve(closes.size() - 1);
    for (size_t i = 1; i < closes.size(); ++i) {
        if (closes[i] <= 0.0 || closes[i - 1] <= 0.0) {
            throw std::invalid_argument("Volatility needs positive prices");
        }
        returns.push_back(std::log(closes[i] / closes[i - 1]));
    }

    double mean = 0.0;
    for (const double r : returns) {
        mean += r;
    }
    mean /= static_cast<double>(returns.size());

    double sumSquaredDifferences = 0.0;
    for (const double r : returns) {
        sumSquaredDifferences += (r - mean) * (r - mean);
    }

    const double dailyVolatility =
        std::sqrt(sumSquaredDifferences / static_cast<double>(returns.size() - 1));
    return dailyVolatility * std::sqrt(TRADING_DAYS);
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
