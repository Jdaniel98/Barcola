#pragma once

#include <cstddef>
#include <vector>

#include "core/price_history.h"

namespace barcola {

class MeanReversion {
public:
    MeanReversion(const PriceHistory& history, int windowSize);

    void calculateMeanReversion();
    void printMeanReversionResults() const;
    [[nodiscard]] double getMeanReversionValue(size_t index) const;
    [[nodiscard]] double getAverageMeanReversion() const;
    [[nodiscard]] size_t getMaxMeanReversionIndex() const;
    [[nodiscard]] size_t getMinMeanReversionIndex() const;
    [[nodiscard]] const std::vector<double>& getValues() const;

private:
    const PriceHistory& priceHistory_;
    int movingWindowSize_;
    std::vector<double> meanReversionValues_;

    [[nodiscard]] double calculateMovingAverage(size_t startIndex) const;
};

}  // namespace barcola
