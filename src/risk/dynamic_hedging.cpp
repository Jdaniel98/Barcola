#include "risk/dynamic_hedging.h"

#include <iostream>
#include <stdexcept>

namespace barcola {

DynamicHedging::DynamicHedging(const PriceHistory& priceHistory) : priceHistory_(priceHistory) {}

DynamicHedging::MAResult DynamicHedging::computeMovingAverages(size_t shortWindow,
                                                                size_t longWindow) const {
    const size_t dataSize = priceHistory_.dataPointsCount();
    if (dataSize < longWindow) {
        return {{}, {}};
    }

    // Compute short MA
    std::vector<double> shortMA(dataSize - shortWindow + 1, 0.0);
    for (size_t i = 0; i < dataSize - shortWindow + 1; ++i) {
        double sum = 0.0;
        for (size_t j = 0; j < shortWindow; ++j) {
            sum += priceHistory_.getDataPoint(i + j).getClosing();
        }
        shortMA[i] = sum / static_cast<double>(shortWindow);
    }

    // FIX Bug 3: longMA gets its OWN loop with correct longWindow bound.
    // Original code shared the shortMA loop and only summed shortWindow elements for longMA.
    std::vector<double> longMA(dataSize - longWindow + 1, 0.0);
    for (size_t i = 0; i < dataSize - longWindow + 1; ++i) {
        double sum = 0.0;
        for (size_t j = 0; j < longWindow; ++j) {
            sum += priceHistory_.getDataPoint(i + j).getClosing();
        }
        longMA[i] = sum / static_cast<double>(longWindow);
    }

    return {shortMA, longMA};
}

void DynamicHedging::performDynamicHedging(size_t shortWindow, size_t longWindow) const {
    const size_t dataSize = priceHistory_.dataPointsCount();
    if (dataSize < longWindow) {
        std::cout << "Insufficient data for hedging." << std::endl;
        return;
    }

    const auto [shortMA, longMA] = computeMovingAverages(shortWindow, longWindow);

    std::cout << "Moving Averages:" << std::endl;
    // Only print where both MAs are valid. The longMA starts at index longWindow-1,
    // shortMA starts at index shortWindow-1. We align by data index.
    const size_t longOffset = longWindow - shortWindow;
    const size_t printCount = std::min(shortMA.size() - longOffset, longMA.size());

    for (size_t i = 0; i < printCount; ++i) {
        const auto currentDataPoint =
            priceHistory_.getDataPoint(i + longWindow - 1);
        std::cout << "Date: " << currentDataPoint.getDateString()
                  << " Short MA: " << shortMA[i + longOffset] << " Long MA: " << longMA[i]
                  << std::endl;
    }
}

}  // namespace barcola
