#pragma once

#include <string>

#include "backtest/strategy.h"

namespace barcola {

// Benchmark: buys on the first bar it evaluates and holds to the end of the backtest.
class BuyAndHoldStrategy : public Strategy {
public:
    Signal evaluate(const BarView& bars, Position position) override;

    [[nodiscard]] std::string name() const override;
};

}  // namespace barcola
