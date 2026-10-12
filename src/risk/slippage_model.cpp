#include "risk/slippage_model.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace barcola {

namespace {

constexpr double BPS_PER_UNIT = 10000.0;

}  // namespace

// FIX: the previous model drew "market volatility" from std::random_device (a different
// answer on every call), gave larger orders less slippage, and returned the trade's price
// move rather than an execution cost.
SlippageModel::SlippageModel(const PriceHistory& priceHistory, double baseBps,
                             double volatilityMultiplier, size_t volatilityWindow)
    : priceHistory_(priceHistory),
      baseFraction_(baseBps / BPS_PER_UNIT),
      volatilityMultiplier_(volatilityMultiplier),
      volatilityWindow_(volatilityWindow) {
    if (!(baseBps >= 0.0)) {
        throw std::invalid_argument("baseBps must be zero or positive");
    }
    if (!(volatilityMultiplier >= 0.0)) {
        throw std::invalid_argument("volatilityMultiplier must be zero or positive");
    }
    if (volatilityWindow < 2) {
        throw std::invalid_argument("volatilityWindow must be at least 2");
    }
}

double SlippageModel::slippageFraction(time_t date) const {
    const auto& points = priceHistory_.getDataPoints();
    const auto it = std::lower_bound(
        points.begin(), points.end(), date,
        [](const PricePoint& point, time_t value) { return point.getDate() < value; });
    if (it == points.end() || it->getDate() != date) {
        throw std::invalid_argument("No bar at epoch " + std::to_string(date));
    }
    const size_t index = static_cast<size_t>(it - points.begin());
    return baseFraction_ + volatilityMultiplier_ * trailingVolatility(index);
}

double SlippageModel::fillPrice(time_t date, double referencePrice, OrderSide side) const {
    const double fraction = slippageFraction(date);
    return side == OrderSide::BUY ? referencePrice * (1.0 + fraction)
                                  : referencePrice * (1.0 - fraction);
}

double SlippageModel::roundTripCost(time_t entryDate, double entryPrice, time_t exitDate,
                                    double exitPrice, double shares) const {
    const double paid = fillPrice(entryDate, entryPrice, OrderSide::BUY);
    const double received = fillPrice(exitDate, exitPrice, OrderSide::SELL);
    return std::abs(shares) * ((paid - entryPrice) + (exitPrice - received));
}

// Sample standard deviation of the log returns between bars [start, barIndex - 1], where
// start keeps at most volatilityWindow_ returns. 0 if fewer than two returns are available.
double SlippageModel::trailingVolatility(size_t barIndex) const {
    if (barIndex < 3) {
        return 0.0;  // bars 0..barIndex-1 hold fewer than two returns
    }
    const auto& points = priceHistory_.getDataPoints();
    const size_t lastBar = barIndex - 1;
    const size_t returnCount = std::min(volatilityWindow_, lastBar);
    const size_t firstBar = lastBar - returnCount;

    std::vector<double> returns;
    returns.reserve(returnCount);
    for (size_t i = firstBar + 1; i <= lastBar; ++i) {
        const double previous = points[i - 1].getClosing();
        const double current = points[i].getClosing();
        if (previous <= 0.0 || current <= 0.0) {
            throw std::invalid_argument("Slippage volatility needs positive prices");
        }
        returns.push_back(std::log(current / previous));
    }

    double mean = 0.0;
    for (const double r : returns) {
        mean += r;
    }
    mean /= static_cast<double>(returns.size());

    double sumSq = 0.0;
    for (const double r : returns) {
        sumSq += (r - mean) * (r - mean);
    }
    return std::sqrt(sumSq / static_cast<double>(returns.size() - 1));
}

}  // namespace barcola
