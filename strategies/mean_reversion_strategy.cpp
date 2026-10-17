#include "mean_reversion_strategy.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace barcola {

MeanReversionStrategy::MeanReversionStrategy(size_t lookbackPeriod, double entryZScore,
                                             double exitZScore, MeanReversionOptions options)
    : lookback_(lookbackPeriod), entryZ_(entryZScore), exitZ_(exitZScore), options_(options) {
    if (lookback_ < 2) {
        throw std::invalid_argument("lookbackPeriod must be at least 2");
    }
    if (!(entryZ_ > 0.0)) {
        throw std::invalid_argument("entryZScore must be positive");
    }
    if (!std::isfinite(exitZ_)) {
        throw std::invalid_argument("exitZScore must be finite");
    }
    if (options_.trendWindow == 1) {
        throw std::invalid_argument("trendWindow must be 0 (off) or at least 2");
    }
}

void MeanReversionStrategy::reset() {
    entryBar_.reset();
}

Signal MeanReversionStrategy::evaluate(const BarView& bars, Position position) {
    // Remember when the current position began, for the time stop
    if (position == Position::LONG) {
        if (!entryBar_) {
            entryBar_ = bars.size() - 1;
        }
    } else {
        entryBar_.reset();
    }

    // Need a full lookback window plus one earlier bar (and a full trend window, if used)
    if (bars.size() <= std::max(lookback_, options_.trendWindow)) {
        return Signal::HOLD;
    }

    const double z = computeZScore(bars);

    if (position == Position::FLAT && z < -entryZ_) {
        // Price is abnormally low — buy, unless the trend filter says the trend is down
        if (options_.trendWindow > 0) {
            double sum = 0.0;
            for (size_t i = bars.size() - options_.trendWindow; i < bars.size(); ++i) {
                sum += bars[i].getClosing();
            }
            const double trend = sum / static_cast<double>(options_.trendWindow);
            if (bars.back().getClosing() <= trend) {
                return Signal::HOLD;
            }
        }
        return Signal::BUY;
    }

    if (position == Position::LONG) {
        if (z > exitZ_) {
            // Price has reverted to (or above) mean — sell
            return Signal::SELL;
        }
        if (options_.maxHoldingBars > 0 &&
            bars.size() - 1 - *entryBar_ >= options_.maxHoldingBars) {
            // Held too long without reverting — sell
            return Signal::SELL;
        }
    }

    return Signal::HOLD;
}

std::string MeanReversionStrategy::name() const {
    std::ostringstream oss;
    oss << "MeanReversion(lookback=" << lookback_
        << ", entryZ=" << entryZ_
        << ", exitZ=" << exitZ_;
    if (options_.trendWindow > 0) {
        oss << ", trend=" << options_.trendWindow;
    }
    if (options_.maxHoldingBars > 0) {
        oss << ", maxHold=" << options_.maxHoldingBars;
    }
    oss << ")";
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
