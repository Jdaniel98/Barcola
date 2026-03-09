#pragma once

#include <cstddef>
#include <vector>

#include "core/price_history.h"

namespace barcola {

class MovingAverage {
public:
    explicit MovingAverage(size_t windowSize);

    void addData(double value);
    [[nodiscard]] double getSMA() const;
    [[nodiscard]] double getEMA(double smoothingFactor) const;

private:
    size_t windowSize_;
    std::vector<double> data_;
};

// Prints SMA/EMA for each data point in the history
void printMovingAverages(const PriceHistory& history, size_t windowSize,
                         double emaSmoothingFactor);

}  // namespace barcola
