#pragma once

#include <cstddef>
#include <string>

#include "backtest/strategy.h"

namespace barcola {

// Trend following with two simple moving averages of the close: long while the short
// average is above the long one, flat while it is below. The rule follows the current state
// rather than waiting for a fresh cross, so a backtest that starts mid-trend joins it.
class MovingAverageCrossoverStrategy : public Strategy {
public:
    // Throws std::invalid_argument unless 0 < shortWindow < longWindow.
    MovingAverageCrossoverStrategy(size_t shortWindow, size_t longWindow);

    Signal evaluate(const BarView& bars, Position position) override;

    [[nodiscard]] std::string name() const override;

private:
    size_t shortWindow_;
    size_t longWindow_;
};

}  // namespace barcola
