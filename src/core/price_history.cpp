#include "core/price_history.h"

#include <cmath>
#include <curl/curl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "core/time_utils.h"

namespace barcola {

namespace {

size_t onDataReceived(char* data, size_t size, size_t nmemb, void* userData) {
    static_cast<std::string*>(userData)->append(data, size * nmemb);
    return size * nmemb;
}

}  // namespace

PriceHistory::PriceHistory(const std::string& assetSymbol) : assetSymbol_(assetSymbol) {}

size_t PriceHistory::dataPointsCount() const {
    return dataPoints_.size();
}

PricePoint PriceHistory::getDataPoint(size_t index) const {
    if (index < dataPoints_.size()) {
        return dataPoints_[index];
    }
    throw std::invalid_argument("Index " + std::to_string(index) +
                                " out of range (size: " + std::to_string(dataPoints_.size()) + ")");
}

PricePoint PriceHistory::getDataPoint(time_t date) const {
    for (const auto& dp : dataPoints_) {
        if (dp.getDate() == date) {
            return dp;
        }
    }
    throw std::invalid_argument("No data point at epoch " + std::to_string(date));
}

PricePoint PriceHistory::getDataPoint(const std::string& date) const {
    for (const auto& dp : dataPoints_) {
        if (dp.getDateString() == date) {
            return dp;
        }
    }
    throw std::invalid_argument("No data point at date " + date);
}

const std::vector<PricePoint>& PriceHistory::getDataPoints() const {
    return dataPoints_;
}

const std::string& PriceHistory::getAssetSymbol() const {
    return assetSymbol_;
}

void PriceHistory::printDataPoints() const {
    for (const auto& dp : dataPoints_) {
        dp.printPricePoint();
    }
}

void PriceHistory::clearDataPoints() {
    dataPoints_.clear();
}

// --- Data loading ---

std::string PriceHistory::fetchYahooCsvData(time_t startTime, time_t endTime,
                                            const std::string& interval) const {
    const std::string url = "https://query1.finance.yahoo.com/v7/finance/download/" +
                            assetSymbol_ + "?period1=" + std::to_string(startTime) +
                            "&period2=" + std::to_string(endTime) + "&interval=" + interval +
                            "&events=history";

    CURL* curlHandle = curl_easy_init();
    if (!curlHandle) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string responseData;
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, onDataReceived);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &responseData);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 30L);

    CURLcode result = curl_easy_perform(curlHandle);
    curl_easy_cleanup(curlHandle);

    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("CURL request failed: ") +
                                 curl_easy_strerror(result));
    }

    return responseData;
}

void PriceHistory::fetchHistoricalData(time_t startDate, time_t endDate,
                                       const std::string& interval) {
    const std::string csvData = fetchYahooCsvData(startDate, endDate, interval);
    std::istringstream csvStream(csvData);
    std::string line;

    // Skip header
    std::getline(csvStream, line);

    while (std::getline(csvStream, line)) {
        std::vector<std::string> fields;
        std::istringstream lineStream(line);
        std::string field;
        while (std::getline(lineStream, field, ',')) {
            fields.push_back(field);
        }

        if (fields.size() >= 5 && fields[0] != "null" && fields[4] != "null") {
            dataPoints_.emplace_back(fields[0], std::stod(fields[1]), std::stod(fields[2]),
                                     std::stod(fields[3]), std::stod(fields[4]));
        }
    }
}

void PriceHistory::fetchHistoricalData(const std::string& startDate, const std::string& endDate,
                                       const std::string& interval) {
    const time_t startTimestamp = convertDateToEpoch(startDate);
    const time_t endTimestamp = convertDateToEpoch(endDate);
    fetchHistoricalData(startTimestamp, endTimestamp, interval);
}

void PriceHistory::loadFromCsv(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open CSV file: " + filepath);
    }

    std::string line;
    // Skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::vector<std::string> fields;
        std::istringstream lineStream(line);
        std::string field;
        while (std::getline(lineStream, field, ',')) {
            fields.push_back(field);
        }

        if (fields.size() >= 5 && fields[0] != "null" && fields[4] != "null") {
            dataPoints_.emplace_back(fields[0], std::stod(fields[1]), std::stod(fields[2]),
                                     std::stod(fields[3]), std::stod(fields[4]));
        }
    }
}

void PriceHistory::saveToCsv(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open CSV file for writing: " + filepath);
    }

    file << "Date,Open,High,Low,Close\n";
    file << std::fixed << std::setprecision(6);
    for (const auto& dp : dataPoints_) {
        file << dp.getDateString() << "," << dp.getOpening() << "," << dp.getHighest() << ","
             << dp.getLowest() << "," << dp.getClosing() << "\n";
    }
}

// --- Technical calculations ---

std::pair<std::vector<double>, std::vector<double>> PriceHistory::calculateSMA(
    size_t windowSize) const {
    const size_t dataSize = dataPoints_.size();
    std::vector<double> smaValues(dataSize, 0.0);

    for (size_t i = windowSize - 1; i < dataSize; ++i) {
        double sum = 0.0;
        for (size_t j = i - windowSize + 1; j <= i; ++j) {
            sum += dataPoints_[j].getClosing();
        }
        smaValues[i] = sum / static_cast<double>(windowSize);
    }

    return {smaValues, {}};
}

std::pair<std::vector<double>, std::vector<double>> PriceHistory::calculateEMA(
    size_t windowSize, double emaSmoothingFactor) const {
    const size_t dataSize = dataPoints_.size();
    if (dataSize < windowSize) {
        return {{}, {}};
    }

    std::vector<double> emaValues(dataSize, 0.0);
    emaValues[windowSize - 1] = dataPoints_[windowSize - 1].getClosing();

    for (size_t i = windowSize; i < dataSize; ++i) {
        emaValues[i] = emaSmoothingFactor * dataPoints_[i].getClosing() +
                       (1.0 - emaSmoothingFactor) * emaValues[i - 1];
    }

    return {{}, emaValues};
}

// FIX: Returns both upper AND lower bands (original only returned upper)
std::pair<std::vector<double>, std::vector<double>> PriceHistory::calculateBollingerBands(
    size_t period, double stdDevFactor) const {
    std::vector<double> bollingerUpper;
    std::vector<double> bollingerLower;
    const size_t dataSize = dataPoints_.size();

    for (size_t i = period - 1; i < dataSize; ++i) {
        double sum = 0.0;
        for (size_t j = i - period + 1; j <= i; ++j) {
            sum += dataPoints_[j].getClosing();
        }
        const double movingAvg = sum / static_cast<double>(period);

        double squaredDiffSum = 0.0;
        for (size_t j = i - period + 1; j <= i; ++j) {
            const double diff = dataPoints_[j].getClosing() - movingAvg;
            squaredDiffSum += diff * diff;
        }
        const double stdDev = std::sqrt(squaredDiffSum / static_cast<double>(period));

        bollingerUpper.push_back(movingAvg + stdDevFactor * stdDev);
        bollingerLower.push_back(movingAvg - stdDevFactor * stdDev);
    }

    return {bollingerUpper, bollingerLower};
}

}  // namespace barcola
