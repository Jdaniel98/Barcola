#include "analysis/correlation.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace barcola {

namespace {

std::vector<double> logReturns(const std::vector<double>& prices) {
    std::vector<double> returns;
    if (prices.size() < 2) {
        return returns;
    }
    returns.reserve(prices.size() - 1);
    for (size_t i = 1; i < prices.size(); ++i) {
        returns.push_back(std::log(prices[i] / prices[i - 1]));
    }
    return returns;
}

}  // namespace

CorrelationAnalysis::CorrelationAnalysis(const PriceHistory& historyA,
                                         const PriceHistory& historyB)
    : historyA_(historyA), historyB_(historyB) {}

double CorrelationAnalysis::calculateCorrelation() const {
    // FIX: rows used to be paired by position, so one missing day shifted every later pair.
    // Join on date instead (both histories are sorted by date).
    const auto& pointsA = historyA_.getDataPoints();
    const auto& pointsB = historyB_.getDataPoints();
    std::vector<double> closesA;
    std::vector<double> closesB;
    size_t i = 0;
    size_t j = 0;
    while (i < pointsA.size() && j < pointsB.size()) {
        if (pointsA[i].getDate() < pointsB[j].getDate()) {
            ++i;
        } else if (pointsB[j].getDate() < pointsA[i].getDate()) {
            ++j;
        } else {
            if (pointsA[i].getClosing() <= 0.0 || pointsB[j].getClosing() <= 0.0) {
                throw std::invalid_argument("Correlation needs positive prices");
            }
            closesA.push_back(pointsA[i].getClosing());
            closesB.push_back(pointsB[j].getClosing());
            ++i;
            ++j;
        }
    }

    // FIX: correlating price levels makes any two trending assets look related.
    // Correlate daily returns instead.
    return barcola::calculateCorrelation(logReturns(closesA), logReturns(closesB));
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
