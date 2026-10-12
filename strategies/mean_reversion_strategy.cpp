#include "mean_reversion_strategy.h"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace barcola {

MeanReversionStrategy::MeanReversionStrategy(size_t lookbackPeriod, double entryZScore,
                                             double exitZScore)
    : lookback_(lookbackPeriod), entryZ_(entryZScore), exitZ_(exitZScore) {
    if (lookback_ < 2) {
        throw std::invalid_argument("lookbackPeriod must be at least 2");
    }
    if (!(entryZ_ > 0.0)) {
        throw std::invalid_argument("entryZScore must be positive");
    }
    if (!std::isfinite(exitZ_)) {
        throw std::invalid_argument("exitZScore must be finite");
    }
}

Signal MeanReversionStrategy::evaluate(const BarView& bars, Position position) {
    // Need a full lookback window plus one earlier bar
    if (bars.size() <= lookback_) {
        return Signal::HOLD;
    }

    const double z = computeZScore(bars);

    if (position == Position::FLAT && z < -entryZ_) {
        // Price is abnormally low — buy
        return Signal::BUY;
    }

    if (position == Position::LONG && z > exitZ_) {
        // Price has reverted to (or above) mean — sell
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

double MeanReversionStrategy::computeZScore(const BarView& bars) const {
    const size_t end = bars.size();
    const size_t start = end - lookback_;

    double sum = 0.0;
    for (size_t i = start; i < end; ++i) {
        sum += bars[i].getClosing();
    }
    const double mean = sum / static_cast<double>(lookback_);

    double sumSqDiff = 0.0;
    for (size_t i = start; i < end; ++i) {
        const double diff = bars[i].getClosing() - mean;
        sumSqDiff += diff * diff;
    }
    const double stdDev = std::sqrt(sumSqDiff / static_cast<double>(lookback_));

    if (stdDev < 1e-12) {
        return 0.0;  // No variance — price is flat
    }

    return (bars.back().getClosing() - mean) / stdDev;
}

}  // namespace barcola
