#pragma once

#include <cstddef>
#include <string>
#include "backtest/strategy.h"

namespace barcola {

class MeanReversionStrategy : public Strategy {
public:
    MeanReversionStrategy(size_t lookbackPeriod,
                          double entryZScore,
                          double exitZScore);

    Signal evaluate(const std::vector<PricePoint>& history,
                    size_t currentIndex) override;

    [[nodiscard]] std::string name() const override;

private:
    size_t lookback_;
    double entryZ_;
    double exitZ_;
    bool inPosition_ = false;

    // Compute z-score of current price vs its lookback-period mean and stddev
    [[nodiscard]] double computeZScore(const std::vector<PricePoint>& history,
                                       size_t currentIndex) const;
};

}  // namespace barcola
