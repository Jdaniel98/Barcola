#pragma once

#include "core/price_history.h"

namespace barcola {

class CorrelationAnalysis {
public:
    CorrelationAnalysis(const PriceHistory& historyA, const PriceHistory& historyB);

    // Pearson correlation coefficient between two price histories.
    // Returns 0.0 if either series has zero variance or insufficient data.
    [[nodiscard]] double calculateCorrelation() const;

private:
    const PriceHistory& historyA_;
    const PriceHistory& historyB_;
};

// Free function: correlation between two price vectors directly.
[[nodiscard]] double calculateCorrelation(const std::vector<double>& a,
                                          const std::vector<double>& b);

}  // namespace barcola
