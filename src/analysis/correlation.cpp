#include "analysis/correlation.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace barcola {

CorrelationAnalysis::CorrelationAnalysis(const PriceHistory& historyA,
                                         const PriceHistory& historyB)
    : historyA_(historyA), historyB_(historyB) {}

double CorrelationAnalysis::calculateCorrelation() const {
    const size_t minDataPoints =
        std::min(historyA_.dataPointsCount(), historyB_.dataPointsCount());

    // FIX Bug 2: Guard against zero or one data point
    if (minDataPoints < 2) {
        return 0.0;
    }

    std::vector<double> pricesA;
    std::vector<double> pricesB;
    pricesA.reserve(minDataPoints);
    pricesB.reserve(minDataPoints);

    for (size_t i = 0; i < minDataPoints; ++i) {
        pricesA.push_back(historyA_.getDataPoint(i).getClosing());
        pricesB.push_back(historyB_.getDataPoint(i).getClosing());
    }

    return barcola::calculateCorrelation(pricesA, pricesB);
}

double calculateCorrelation(const std::vector<double>& a, const std::vector<double>& b) {
    const size_t n = std::min(a.size(), b.size());
    if (n < 2) {
        return 0.0;
    }

    const double meanA = std::accumulate(a.begin(), a.begin() + n, 0.0) / static_cast<double>(n);
    const double meanB = std::accumulate(b.begin(), b.begin() + n, 0.0) / static_cast<double>(n);

    double covariance = 0.0;
    double varianceA = 0.0;
    double varianceB = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double deviationA = a[i] - meanA;
        const double deviationB = b[i] - meanB;

        covariance += deviationA * deviationB;
        varianceA += deviationA * deviationA;
        varianceB += deviationB * deviationB;
    }

    covariance /= static_cast<double>(n);
    varianceA /= static_cast<double>(n);
    varianceB /= static_cast<double>(n);

    // FIX Bug 2: Guard against division by zero when variance is zero
    const double denominator = std::sqrt(varianceA) * std::sqrt(varianceB);
    if (denominator < 1e-10) {
        return 0.0;
    }

    return covariance / denominator;
}

}  // namespace barcola
