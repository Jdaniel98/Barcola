#pragma once

#include <cstddef>
#include <string>

#include "backtest/strategy.h"

namespace barcola {

// Buys when the close is more than entryZScore standard deviations below its lookback mean,
// and sells once it is more than exitZScore standard deviations above it. Stateless: the
// current position comes from the engine.
class MeanReversionStrategy : public Strategy {
public:
    // Throws std::invalid_argument if lookbackPeriod < 2 or entryZScore <= 0.
    MeanReversionStrategy(size_t lookbackPeriod, double entryZScore, double exitZScore);

    Signal evaluate(const BarView& bars, Position position) override;

    [[nodiscard]] std::string name() const override;

private:
    size_t lookback_;
    double entryZ_;
    double exitZ_;

    // z-score of the latest close against the mean and standard deviation of the last
    // lookback_ closes (the latest one included).
    [[nodiscard]] double computeZScore(const BarView& bars) const;
};

}  // namespace barcola
