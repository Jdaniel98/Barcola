#pragma once

#include <cstddef>
#include <vector>

#include "core/price_history.h"

namespace barcola {

// Deviation of each close from its trailing simple moving average (close - SMA).
class MeanReversion {
public:
    // Throws std::invalid_argument if windowSize < 1.
    MeanReversion(const PriceHistory& history, int windowSize);

    void calculateMeanReversion();
    void printMeanReversionResults() const;
    [[nodiscard]] double getMeanReversionValue(size_t index) const;
    [[nodiscard]] double getAverageMeanReversion() const;
    [[nodiscard]] size_t getMaxMeanReversionIndex() const;
    [[nodiscard]] size_t getMinMeanReversionIndex() const;
    // One value per bar from index windowSize - 1 onward.
    [[nodiscard]] const std::vector<double>& getValues() const;

private:
    const PriceHistory& priceHistory_;
    int movingWindowSize_;
    std::vector<double> meanReversionValues_;
};

}  // namespace barcola
