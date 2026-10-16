#include "ma_crossover_strategy.h"

#include <sstream>
#include <stdexcept>

namespace barcola {

namespace {

// Mean of the last `window` closes; the caller guarantees bars.size() >= window.
double trailingMean(const BarView& bars, size_t window) {
    double sum = 0.0;
    for (size_t i = bars.size() - window; i < bars.size(); ++i) {
        sum += bars[i].getClosing();
    }
    return sum / static_cast<double>(window);
}

}  // namespace

MovingAverageCrossoverStrategy::MovingAverageCrossoverStrategy(size_t shortWindow,
                                                               size_t longWindow)
    : shortWindow_(shortWindow), longWindow_(longWindow) {
    if (shortWindow_ == 0 || shortWindow_ >= longWindow_) {
        throw std::invalid_argument("Moving average windows must satisfy 0 < short < long");
    }
}

Signal MovingAverageCrossoverStrategy::evaluate(const BarView& bars, Position position) {
    if (bars.size() < longWindow_) {
        return Signal::HOLD;
    }

    const double shortMA = trailingMean(bars, shortWindow_);
    const double longMA = trailingMean(bars, longWindow_);

    if (position == Position::FLAT && shortMA > longMA) {
        return Signal::BUY;
    }
    if (position == Position::LONG && shortMA < longMA) {
        return Signal::SELL;
    }
    return Signal::HOLD;
}

std::string MovingAverageCrossoverStrategy::name() const {
    std::ostringstream oss;
    oss << "MACrossover(short=" << shortWindow_ << ", long=" << longWindow_ << ")";
    return oss.str();
}

}  // namespace barcola
