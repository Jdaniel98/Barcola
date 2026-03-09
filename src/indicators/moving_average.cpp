#include "indicators/moving_average.h"

#include <iostream>

namespace barcola {

MovingAverage::MovingAverage(size_t windowSize) : windowSize_(windowSize) {}

void MovingAverage::addData(double value) {
    data_.push_back(value);
    if (data_.size() > windowSize_) {
        data_.erase(data_.begin());
    }
}

double MovingAverage::getSMA() const {
    if (data_.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (const double value : data_) {
        sum += value;
    }
    return sum / static_cast<double>(data_.size());
}

// FIX Bug 4: Original used backward size_t loop that could underflow.
// Now uses forward iteration which is mathematically equivalent for EMA.
double MovingAverage::getEMA(double smoothingFactor) const {
    if (data_.empty()) {
        return 0.0;
    }
    if (data_.size() == 1) {
        return data_[0];
    }

    // Forward EMA: start from oldest, accumulate toward newest
    double ema = data_[0];
    for (size_t i = 1; i < data_.size(); ++i) {
        ema = smoothingFactor * data_[i] + (1.0 - smoothingFactor) * ema;
    }
    return ema;
}

void printMovingAverages(const PriceHistory& history, size_t windowSize,
                         double emaSmoothingFactor) {
    MovingAverage sma(windowSize);
    MovingAverage ema(windowSize);

    const auto& dataPoints = history.getDataPoints();
    for (size_t i = 0; i < dataPoints.size(); ++i) {
        const double closingPrice = dataPoints[i].getClosing();
        sma.addData(closingPrice);
        ema.addData(closingPrice);

        std::cout << "Date: " << dataPoints[i].getDateString()
                  << " Closing Price: " << closingPrice << " SMA: " << sma.getSMA()
                  << " EMA: " << ema.getEMA(emaSmoothingFactor) << std::endl;
    }
}

}  // namespace barcola
