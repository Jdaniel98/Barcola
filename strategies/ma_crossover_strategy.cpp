#include "ma_crossover_strategy.h"

#include <sstream>
#include <stdexcept>

#include "detail/window_stats.h"

namespace barcola {

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

    const double shortMA = detail::trailingMean(bars, shortWindow_);
    const double longMA = detail::trailingMean(bars, longWindow_);

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
