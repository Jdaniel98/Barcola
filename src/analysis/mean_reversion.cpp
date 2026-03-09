#include "analysis/mean_reversion.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace barcola {

MeanReversion::MeanReversion(const PriceHistory& history, int windowSize)
    : priceHistory_(history), movingWindowSize_(windowSize) {}

double MeanReversion::calculateMovingAverage(size_t startIndex) const {
    const auto& dataPoints = priceHistory_.getDataPoints();
    const size_t endIndex = startIndex + static_cast<size_t>(movingWindowSize_);

    if (endIndex > dataPoints.size()) {
        throw std::out_of_range("Window extends beyond data range");
    }

    double sum = 0.0;
    for (size_t i = startIndex; i < endIndex; ++i) {
        sum += dataPoints[i].getClosing();
    }
    return sum / static_cast<double>(movingWindowSize_);
}

void MeanReversion::calculateMeanReversion() {
    const auto& dataPoints = priceHistory_.getDataPoints();
    const size_t dataSize = dataPoints.size();
    const size_t windowSize = static_cast<size_t>(movingWindowSize_);

    meanReversionValues_.clear();

    if (dataSize < windowSize) {
        return;
    }

    // For each point starting at windowSize-1, compute deviation from its local moving average
    for (size_t i = windowSize - 1; i < dataSize; ++i) {
        const size_t startIdx = i - windowSize + 1;
        const double movingAvg = calculateMovingAverage(startIdx);
        const double deviation = dataPoints[i].getClosing() - movingAvg;
        meanReversionValues_.push_back(deviation);
    }
}

void MeanReversion::printMeanReversionResults() const {
    const auto& dataPoints = priceHistory_.getDataPoints();
    const size_t windowSize = static_cast<size_t>(movingWindowSize_);

    for (size_t i = 0; i < meanReversionValues_.size(); ++i) {
        const size_t dataIdx = i + windowSize - 1;
        std::cout << "Date: " << dataPoints[dataIdx].getDateString()
                  << " Deviation: " << meanReversionValues_[i] << std::endl;
    }
}

double MeanReversion::getMeanReversionValue(size_t index) const {
    if (index < meanReversionValues_.size()) {
        return meanReversionValues_[index];
    }
    throw std::out_of_range("Index out of range");
}

double MeanReversion::getAverageMeanReversion() const {
    if (meanReversionValues_.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (const double value : meanReversionValues_) {
        sum += value;
    }
    return sum / static_cast<double>(meanReversionValues_.size());
}

size_t MeanReversion::getMaxMeanReversionIndex() const {
    if (meanReversionValues_.empty()) {
        throw std::out_of_range("No mean reversion values computed");
    }
    return static_cast<size_t>(std::distance(
        meanReversionValues_.begin(),
        std::max_element(meanReversionValues_.begin(), meanReversionValues_.end())));
}

size_t MeanReversion::getMinMeanReversionIndex() const {
    if (meanReversionValues_.empty()) {
        throw std::out_of_range("No mean reversion values computed");
    }
    return static_cast<size_t>(std::distance(
        meanReversionValues_.begin(),
        std::min_element(meanReversionValues_.begin(), meanReversionValues_.end())));
}

const std::vector<double>& MeanReversion::getValues() const {
    return meanReversionValues_;
}

}  // namespace barcola
