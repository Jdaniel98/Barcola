#pragma once

#include "core/price_history.h"

namespace barcola {

class CorrelationAnalysis {
public:
    CorrelationAnalysis(const PriceHistory& historyA, const PriceHistory& historyB);

    // Pearson correlation of the two assets' daily log returns, using only dates present in
    // both histories. Returns 0.0 if there are fewer than two common returns or either series
    // has zero variance. Throws std::invalid_argument on a non-positive price.
    [[nodiscard]] double calculateCorrelation() const;

private:
    const PriceHistory& historyA_;
    const PriceHistory& historyB_;
};

// Pearson correlation coefficient of two series, paired by position. Uses the first
// min(a.size(), b.size()) values. Returns 0.0 for fewer than two pairs or zero variance.
[[nodiscard]] double calculateCorrelation(const std::vector<double>& a,
                                          const std::vector<double>& b);

}  // namespace barcola
