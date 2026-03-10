#include "mean_reversion_strategy.h"

#include <cmath>
#include <sstream>

namespace barcola {

MeanReversionStrategy::MeanReversionStrategy(size_t lookbackPeriod,
                                             double entryZScore,
                                             double exitZScore)
    : lookback_(lookbackPeriod), entryZ_(entryZScore), exitZ_(exitZScore) {}

Signal MeanReversionStrategy::evaluate(const std::vector<PricePoint>& history,
                                       size_t currentIndex) {
    // Need enough history for lookback window
    if (currentIndex < lookback_) {
        return Signal::HOLD;
    }

    const double z = computeZScore(history, currentIndex);

    if (!inPosition_ && z < -entryZ_) {
        // Price is abnormally low — buy
        inPosition_ = true;
        return Signal::BUY;
    }

    if (inPosition_ && z > exitZ_) {
        // Price has reverted to (or above) mean — sell
        inPosition_ = false;
        return Signal::SELL;
    }

    return Signal::HOLD;
}

std::string MeanReversionStrategy::name() const {
    std::ostringstream oss;
    oss << "MeanReversion(lookback=" << lookback_
        << ", entryZ=" << entryZ_
        << ", exitZ=" << exitZ_ << ")";
    return oss.str();
}

double MeanReversionStrategy::computeZScore(
    const std::vector<PricePoint>& history,
    size_t currentIndex) const {

    // Compute mean over the lookback window
    double sum = 0.0;
    const size_t start = currentIndex - lookback_ + 1;
    for (size_t i = start; i <= currentIndex; ++i) {
        sum += history[i].getClosing();
    }
    const double mean = sum / static_cast<double>(lookback_);

    // Compute standard deviation
    double sumSqDiff = 0.0;
    for (size_t i = start; i <= currentIndex; ++i) {
        const double diff = history[i].getClosing() - mean;
        sumSqDiff += diff * diff;
    }
    const double stdDev = std::sqrt(sumSqDiff / static_cast<double>(lookback_));

    if (stdDev < 1e-12) {
        return 0.0;  // No variance — price is flat
    }

    return (history[currentIndex].getClosing() - mean) / stdDev;
}

}  // namespace barcola
