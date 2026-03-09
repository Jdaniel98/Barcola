#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/price_point.h"

namespace barcola {

class PriceHistory {
public:
    explicit PriceHistory(const std::string& assetSymbol);
    ~PriceHistory() = default;

    // Data access
    [[nodiscard]] size_t dataPointsCount() const;
    [[nodiscard]] PricePoint getDataPoint(size_t index) const;
    [[nodiscard]] PricePoint getDataPoint(time_t date) const;
    [[nodiscard]] PricePoint getDataPoint(const std::string& date) const;
    [[nodiscard]] const std::vector<PricePoint>& getDataPoints() const;
    [[nodiscard]] const std::string& getAssetSymbol() const;
    void printDataPoints() const;
    void clearDataPoints();

    // Data loading
    void fetchHistoricalData(time_t startDate, time_t endDate, const std::string& interval);
    void fetchHistoricalData(const std::string& startDate, const std::string& endDate,
                             const std::string& interval);
    void loadFromCsv(const std::string& filepath);
    void saveToCsv(const std::string& filepath) const;

    // Technical calculations (kept on PriceHistory for backward compatibility)
    [[nodiscard]] std::pair<std::vector<double>, std::vector<double>> calculateSMA(
        size_t windowSize) const;
    [[nodiscard]] std::pair<std::vector<double>, std::vector<double>> calculateEMA(
        size_t windowSize, double emaSmoothingFactor) const;
    [[nodiscard]] std::pair<std::vector<double>, std::vector<double>> calculateBollingerBands(
        size_t period, double stdDevFactor) const;

private:
    std::string assetSymbol_;
    std::vector<PricePoint> dataPoints_;

    [[nodiscard]] std::string fetchYahooCsvData(time_t startTime, time_t endTime,
                                                const std::string& interval) const;
};

}  // namespace barcola
