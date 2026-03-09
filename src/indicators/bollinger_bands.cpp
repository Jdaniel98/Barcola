#include "indicators/bollinger_bands.h"

#include <cmath>

namespace barcola {

std::pair<std::vector<double>, std::vector<double>> calculateBollingerBands(
    const std::vector<double>& prices, size_t period, double stdDevFactor) {
    std::vector<double> upper;
    std::vector<double> lower;

    if (prices.size() < period) {
        return {upper, lower};
    }

    for (size_t i = period - 1; i < prices.size(); ++i) {
        double sum = 0.0;
        for (size_t j = i - period + 1; j <= i; ++j) {
            sum += prices[j];
        }
        const double movingAvg = sum / static_cast<double>(period);

        double squaredDiffSum = 0.0;
        for (size_t j = i - period + 1; j <= i; ++j) {
            const double diff = prices[j] - movingAvg;
            squaredDiffSum += diff * diff;
        }
        const double stdDev = std::sqrt(squaredDiffSum / static_cast<double>(period));

        upper.push_back(movingAvg + stdDevFactor * stdDev);
        lower.push_back(movingAvg - stdDevFactor * stdDev);
    }

    return {upper, lower};
}

}  // namespace barcola
