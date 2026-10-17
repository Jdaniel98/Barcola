#include "risk/dynamic_hedging.h"

#include <iostream>
#include <stdexcept>

#include "indicators/moving_average.h"

namespace barcola {

namespace {

void requireWindows(size_t shortWindow, size_t longWindow) {
    // FIX: shortWindow >= longWindow used to wrap an unsigned offset and read out of bounds
    if (shortWindow == 0 || shortWindow >= longWindow) {
        throw std::invalid_argument("Moving average windows must satisfy 0 < short < long");
    }
}

// SMA without the NaN warm-up: element k covers values [k, k + window).
std::vector<double> trimmedSMA(const std::vector<double>& values, size_t window) {
    const std::vector<double> sma = calculateSMA(values, window);
    return {sma.begin() + static_cast<std::ptrdiff_t>(window - 1), sma.end()};
}

}  // namespace

DynamicHedging::DynamicHedging(const PriceHistory& priceHistory) : priceHistory_(priceHistory) {}

DynamicHedging::MAResult DynamicHedging::computeMovingAverages(size_t shortWindow,
                                                               size_t longWindow) const {
    requireWindows(shortWindow, longWindow);

    const std::vector<double> closes = priceHistory_.getClosingPrices();
    if (closes.size() < longWindow) {
        return {{}, {}};
    }

    // FIX Bug 3: longMA uses the full longWindow (it used to sum only shortWindow elements)
    return {trimmedSMA(closes, shortWindow), trimmedSMA(closes, longWindow)};
}

void DynamicHedging::performDynamicHedging(size_t shortWindow, size_t longWindow) const {
    requireWindows(shortWindow, longWindow);

    const size_t dataSize = priceHistory_.dataPointsCount();
    if (dataSize < longWindow) {
        std::cout << "Insufficient data for hedging." << '\n';
        return;
    }

    const auto [shortMA, longMA] = computeMovingAverages(shortWindow, longWindow);

    std::cout << "Moving Averages:" << '\n';
    // longMA[i] ends at bar i + longWindow - 1; the shortMA ending at the same bar is offset
    // by longWindow - shortWindow.
    const size_t longOffset = longWindow - shortWindow;
    for (size_t i = 0; i < longMA.size(); ++i) {
        const auto currentDataPoint = priceHistory_.getDataPoint(i + longWindow - 1);
        std::cout << "Date: " << currentDataPoint.getDateString()
                  << " Short MA: " << shortMA[i + longOffset] << " Long MA: " << longMA[i] << '\n';
    }
}

}  // namespace barcola
