#include "risk/position_sizing.h"

#include <stdexcept>

namespace barcola {

PositionSizing::PositionSizing(double riskPercentage, double stopLossPercentage)
    : riskPercentage_(riskPercentage), stopLossPercentage_(stopLossPercentage) {}

double PositionSizing::calculatePositionSize(double portfolioSize, double entryPrice) const {
    const double stopLossAmount = entryPrice * stopLossPercentage_;
    if (stopLossAmount < 1e-10) {
        throw std::invalid_argument("Stop loss amount is effectively zero");
    }
    const double riskAmount = portfolioSize * riskPercentage_;
    return riskAmount / stopLossAmount;
}

double PositionSizing::calculatePositionSizeWithMaxLoss(double portfolioSize, double entryPrice,
                                                        double maxLossAmount) const {
    const double denominator = entryPrice * stopLossPercentage_;
    if (denominator < 1e-10) {
        throw std::invalid_argument("Stop loss amount is effectively zero");
    }
    return maxLossAmount / denominator;
}

void PositionSizing::setRiskPercentage(double percentage) {
    riskPercentage_ = percentage;
}

void PositionSizing::setStopLossPercentage(double percentage) {
    stopLossPercentage_ = percentage;
}

double PositionSizing::getRiskPercentage() const {
    return riskPercentage_;
}

double PositionSizing::getStopLossPercentage() const {
    return stopLossPercentage_;
}

}  // namespace barcola
