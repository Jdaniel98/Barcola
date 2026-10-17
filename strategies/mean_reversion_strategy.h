#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "backtest/strategy.h"

namespace barcola {

// Optional refinements, both off by default.
struct MeanReversionOptions {
    // Only buy while the close is above its trailing average over this many bars, i.e. buy
    // dips in uptrends only. 0 turns the filter off.
    size_t trendWindow = 0;
    // Sell once a position has been held this many bars, even if the price hasn't reverted.
    // Bars are counted from the first bar the strategy sees itself long (the fill bar with
    // next-open fills). 0 turns the time stop off.
    size_t maxHoldingBars = 0;
};

// Buys when the close is more than entryZScore standard deviations below its lookback mean,
// and sells once it is more than exitZScore standard deviations above it. The current
// position comes from the engine; the only state kept is when the current position began,
// for the time stop, and reset() clears it.
class MeanReversionStrategy : public Strategy {
public:
    // Throws std::invalid_argument if lookbackPeriod < 2, entryZScore <= 0, or
    // options.trendWindow is 1.
    MeanReversionStrategy(size_t lookbackPeriod, double entryZScore, double exitZScore,
                          MeanReversionOptions options = {});

    void reset() override;

    Signal evaluate(const BarView& bars, Position position) override;

    [[nodiscard]] std::string name() const override;

private:
    size_t lookback_;
    double entryZ_;
    double exitZ_;
    MeanReversionOptions options_;
    std::optional<size_t> entryBar_;  // index of the first bar seen long, while long

    // z-score of the latest close against the mean and standard deviation of the last
    // lookback_ closes (the latest one included).
    [[nodiscard]] double computeZScore(const BarView& bars) const;
};

}  // namespace barcola
