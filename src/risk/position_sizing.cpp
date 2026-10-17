#include "risk/position_sizing.h"

#include <algorithm>
#include <stdexcept>

namespace barcola {

namespace {

void requireRisk(double percentage) {
    if (!(percentage > 0.0 && percentage <= 1.0)) {
        throw std::invalid_argument("Risk percentage must be in (0, 1]");
    }
}

void requireStopLoss(double percentage) {
    if (!(percentage > 0.0 && percentage < 1.0)) {
        throw std::invalid_argument("Stop loss percentage must be in (0, 1)");
    }
}

void requireTradeInputs(double portfolioSize, double entryPrice) {
    if (!(portfolioSize > 0.0)) {
        throw std::invalid_argument("Portfolio size must be positive");
    }
    if (!(entryPrice > 0.0)) {
        throw std::invalid_argument("Entry price must be positive");
    }
}

}  // namespace

PositionSizing::PositionSizing(double riskPercentage, double stopLossPercentage, double maxLeverage)
    : riskPercentage_(riskPercentage),
      stopLossPercentage_(stopLossPercentage),
      maxLeverage_(maxLeverage) {
    requireRisk(riskPercentage);
    requireStopLoss(stopLossPercentage);
    if (!(maxLeverage > 0.0)) {
        throw std::invalid_argument("Max leverage must be positive");
    }
}

double PositionSizing::calculatePositionSize(double portfolioSize, double entryPrice) const {
    requireTradeInputs(portfolioSize, entryPrice);
    const double riskAmount = portfolioSize * riskPercentage_;
    const double shares = riskAmount / (entryPrice * stopLossPercentage_);
    return capToLeverage(shares, portfolioSize, entryPrice);
}

double PositionSizing::calculatePositionSizeWithMaxLoss(double portfolioSize, double entryPrice,
                                                        double maxLossAmount) const {
    requireTradeInputs(portfolioSize, entryPrice);
    if (!(maxLossAmount >= 0.0)) {
        throw std::invalid_argument("Max loss amount must be zero or positive");
    }
    const double shares = maxLossAmount / (entryPrice * stopLossPercentage_);
    return capToLeverage(shares, portfolioSize, entryPrice);
}

// FIX: a tight stop used to produce positions worth several times the portfolio without
// any warning (2% risk with a 1% stop meant 200% of the portfolio).
double PositionSizing::capToLeverage(double shares, double portfolioSize, double entryPrice) const {
    const double maxShares = portfolioSize * maxLeverage_ / entryPrice;
    return std::min(shares, maxShares);
}

void PositionSizing::setRiskPercentage(double percentage) {
    requireRisk(percentage);
    riskPercentage_ = percentage;
}

void PositionSizing::setStopLossPercentage(double percentage) {
    requireStopLoss(percentage);
    stopLossPercentage_ = percentage;
}

double PositionSizing::getRiskPercentage() const {
    return riskPercentage_;
}

double PositionSizing::getStopLossPercentage() const {
    return stopLossPercentage_;
}

double PositionSizing::getMaxLeverage() const {
    return maxLeverage_;
}

}  // namespace barcola
