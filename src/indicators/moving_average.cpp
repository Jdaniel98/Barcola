#include "indicators/moving_average.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace barcola {

namespace {

constexpr double NOT_ENOUGH_DATA = std::numeric_limits<double>::quiet_NaN();

void requireWindow(size_t window) {
    if (window == 0) {
        throw std::invalid_argument("Moving average window must be at least 1");
    }
}

}  // namespace

std::vector<double> calculateSMA(const std::vector<double>& values, size_t window) {
    requireWindow(window);
    std::vector<double> sma(values.size(), NOT_ENOUGH_DATA);
    if (values.size() < window) {
        return sma;
    }

    // Recompute each window's sum rather than keeping a running total, so rounding error
    // can't accumulate over long series.
    for (size_t i = window - 1; i < values.size(); ++i) {
        double sum = 0.0;
        for (size_t j = i + 1 - window; j <= i; ++j) {
            sum += values[j];
        }
        sma[i] = sum / static_cast<double>(window);
    }
    return sma;
}

std::vector<double> calculateEMA(const std::vector<double>& values, size_t window) {
    requireWindow(window);
    std::vector<double> ema(values.size(), NOT_ENOUGH_DATA);
    if (values.size() < window) {
        return ema;
    }

    double seed = 0.0;
    for (size_t i = 0; i < window; ++i) {
        seed += values[i];
    }
    ema[window - 1] = seed / static_cast<double>(window);

    const double alpha = 2.0 / (static_cast<double>(window) + 1.0);
    for (size_t i = window; i < values.size(); ++i) {
        ema[i] = alpha * values[i] + (1.0 - alpha) * ema[i - 1];
    }
    return ema;
}

MovingAverage::MovingAverage(size_t windowSize) : windowSize_(windowSize) {
    requireWindow(windowSize);
}

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
    if (!(smoothingFactor > 0.0 && smoothingFactor <= 1.0)) {
        throw std::invalid_argument("EMA smoothing factor must be in (0, 1]");
    }
    if (data_.empty()) {
        return 0.0;
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
                  << " EMA: " << ema.getEMA(emaSmoothingFactor) << '\n';
    }
}

}  // namespace barcola
