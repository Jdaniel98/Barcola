#include "core/price_history.h"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "core/time_utils.h"

namespace barcola {

namespace {

constexpr long HTTP_OK = 200;
constexpr long CURL_TIMEOUT_SECONDS = 30;

size_t onDataReceived(char* data, size_t size, size_t nmemb, void* userData) {
    static_cast<std::string*>(userData)->append(data, size * nmemb);
    return size * nmemb;
}

struct CurlDeleter {
    void operator()(CURL* handle) const { curl_easy_cleanup(handle); }
};
using CurlHandle = std::unique_ptr<CURL, CurlDeleter>;

std::string trim(const std::string& text) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    const auto first = std::find_if_not(text.begin(), text.end(), isSpace);
    const auto last = std::find_if_not(text.rbegin(), text.rend(), isSpace).base();
    return first < last ? std::string(first, last) : std::string();
}

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream lineStream(line);
    std::string field;
    while (std::getline(lineStream, field, ',')) {
        fields.push_back(trim(field));
    }
    return fields;
}

struct CsvColumns {
    size_t date = 0;
    size_t open = 0;
    size_t high = 0;
    size_t low = 0;
    size_t close = 0;
    std::optional<size_t> adjClose;
};

CsvColumns findColumns(const std::vector<std::string>& header, const std::string& source) {
    const auto find = [&](const std::string& name) -> std::optional<size_t> {
        for (size_t i = 0; i < header.size(); ++i) {
            if (toLower(header[i]) == name) {
                return i;
            }
        }
        return std::nullopt;
    };
    const auto require = [&](const std::string& name) {
        const auto index = find(name);
        if (!index) {
            throw std::runtime_error(source + ": missing \"" + name + "\" column in header");
        }
        return *index;
    };

    CsvColumns columns;
    columns.date = require("date");
    columns.open = require("open");
    columns.high = require("high");
    columns.low = require("low");
    columns.close = require("close");
    columns.adjClose = find("adj close");
    return columns;
}

double parseNumber(const std::string& field, const std::string& where) {
    size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(field, &consumed);
    } catch (const std::exception&) {
        consumed = 0;
    }
    if (consumed == 0 || consumed != field.size()) {
        throw std::runtime_error(where + ": not a number: \"" + field + "\"");
    }
    return value;
}

// Parses Yahoo-style OHLC CSV into bars sorted by date.
std::vector<PricePoint> parseCsv(std::istream& input, const std::string& source) {
    std::vector<PricePoint> points;
    std::string line;
    if (!std::getline(input, line)) {
        return points;  // Completely empty input
    }
    const CsvColumns columns = findColumns(splitCsvLine(line), source);
    const size_t maxColumn = std::max({columns.date, columns.open, columns.high, columns.low,
                                       columns.close, columns.adjClose.value_or(0)});

    size_t lineNumber = 1;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (trim(line).empty()) {
            continue;
        }
        const auto fields = splitCsvLine(line);
        const std::string where = source + ":" + std::to_string(lineNumber);
        if (fields.size() <= maxColumn) {
            throw std::runtime_error(where + ": expected at least " +
                                     std::to_string(maxColumn + 1) + " fields");
        }

        // Yahoo marks missing values with "null"; skip those rows
        std::vector<size_t> used = {columns.date, columns.open, columns.high, columns.low,
                                    columns.close};
        if (columns.adjClose) {
            used.push_back(*columns.adjClose);
        }
        const bool hasNull = std::any_of(used.begin(), used.end(), [&](size_t column) {
            return fields[column] == "null" || fields[column].empty();
        });
        if (hasNull) {
            continue;
        }

        double open = parseNumber(fields[columns.open], where);
        double high = parseNumber(fields[columns.high], where);
        double low = parseNumber(fields[columns.low], where);
        double close = parseNumber(fields[columns.close], where);
        if (columns.adjClose && close != 0.0) {
            const double factor = parseNumber(fields[*columns.adjClose], where) / close;
            open *= factor;
            high *= factor;
            low *= factor;
            close *= factor;
        }

        try {
            points.emplace_back(fields[columns.date], open, high, low, close);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(where + ": " + e.what());
        }
    }

    std::stable_sort(points.begin(), points.end(), [](const PricePoint& a, const PricePoint& b) {
        return a.getDate() < b.getDate();
    });
    const auto duplicate = std::adjacent_find(
        points.begin(), points.end(),
        [](const PricePoint& a, const PricePoint& b) { return a.getDate() == b.getDate(); });
    if (duplicate != points.end()) {
        throw std::runtime_error(source + ": duplicate date " + duplicate->getDateString());
    }
    return points;
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
    const auto it = std::lower_bound(
        dataPoints_.begin(), dataPoints_.end(), date,
        [](const PricePoint& point, time_t value) { return point.getDate() < value; });
    if (it != dataPoints_.end() && it->getDate() == date) {
        return *it;
    }
    throw std::invalid_argument("No data point at epoch " + std::to_string(date));
}

PricePoint PriceHistory::getDataPoint(const std::string& date) const {
    try {
        return getDataPoint(convertDateToEpoch(date));
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument("No data point at date " + date);
    }
}

const std::vector<PricePoint>& PriceHistory::getDataPoints() const {
    return dataPoints_;
}

std::vector<double> PriceHistory::getClosingPrices() const {
    std::vector<double> closes;
    closes.reserve(dataPoints_.size());
    for (const auto& dp : dataPoints_) {
        closes.push_back(dp.getClosing());
    }
    return closes;
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

void PriceHistory::addDataPoint(const PricePoint& point) {
    if (!dataPoints_.empty() && point.getDate() <= dataPoints_.back().getDate()) {
        throw std::invalid_argument("Bar dated " + point.getDateString() +
                                    " is not after the last bar (" +
                                    dataPoints_.back().getDateString() + ")");
    }
    dataPoints_.push_back(point);
}

// --- Data loading ---

std::string PriceHistory::fetchYahooCsvData(time_t startTime, time_t endTime,
                                            const std::string& interval) const {
    const std::string url = "https://query1.finance.yahoo.com/v7/finance/download/" +
                            assetSymbol_ + "?period1=" + std::to_string(startTime) +
                            "&period2=" + std::to_string(endTime) + "&interval=" + interval +
                            "&events=history";

    const CurlHandle curl(curl_easy_init());
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string responseData;
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, onDataReceived);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &responseData);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, CURL_TIMEOUT_SECONDS);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

    const CURLcode result = curl_easy_perform(curl.get());
    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("CURL request failed: ") +
                                 curl_easy_strerror(result));
    }

    // FIX: an error page used to be parsed as CSV and silently produced zero rows
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (status != HTTP_OK) {
        throw std::runtime_error("Yahoo Finance returned HTTP " + std::to_string(status) +
                                 " for " + assetSymbol_ +
                                 ". The v7 download endpoint may require authentication; "
                                 "save the data as CSV and use loadFromCsv instead.");
    }

    return responseData;
}

void PriceHistory::fetchHistoricalData(time_t startDate, time_t endDate,
                                       const std::string& interval) {
    std::istringstream csvStream(fetchYahooCsvData(startDate, endDate, interval));
    auto points = parseCsv(csvStream, "Yahoo Finance response for " + assetSymbol_);
    if (points.empty()) {
        throw std::runtime_error("Yahoo Finance returned no price rows for " + assetSymbol_);
    }
    dataPoints_ = std::move(points);
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
    dataPoints_ = parseCsv(file, filepath);
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

}  // namespace barcola
