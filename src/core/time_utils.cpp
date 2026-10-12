#include "core/time_utils.h"

#include <cctype>
#include <cstdio>
#include <stdexcept>

namespace barcola {

namespace {

constexpr size_t DATE_LENGTH = 10;  // YYYY-MM-DD

bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month) {
    constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (month == 2 && isLeapYear(year)) ? 29 : kDays[month - 1];
}

}  // namespace

time_t getCurrentEpoch() {
    return time(nullptr);
}

time_t convertDateToEpoch(const std::string& date) {
    if (date.size() < DATE_LENGTH || date[4] != '-' || date[7] != '-') {
        throw std::invalid_argument("Date must be in YYYY-MM-DD format: " + date);
    }
    for (size_t i = 0; i < DATE_LENGTH; ++i) {
        if (i != 4 && i != 7 && !std::isdigit(static_cast<unsigned char>(date[i]))) {
            throw std::invalid_argument("Date must be in YYYY-MM-DD format: " + date);
        }
    }

    int year = 0, month = 0, day = 0;
    if (std::sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        throw std::invalid_argument("Failed to parse date: " + date);
    }

    // FIX: days past the end of the month (e.g. 2023-02-31) used to roll into the next month
    if (month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month)) {
        throw std::invalid_argument("Invalid date components: " + date);
    }

    struct tm timeStruct = {};
    timeStruct.tm_year = year - 1900;
    timeStruct.tm_mon = month - 1;
    timeStruct.tm_mday = day;

#ifdef _WIN32
    return _mkgmtime(&timeStruct);
#else
    return timegm(&timeStruct);
#endif
}

std::string convertEpochToDate(time_t epoch) {
    struct tm timeInfo = {};
#ifdef _WIN32
    const bool ok = gmtime_s(&timeInfo, &epoch) == 0;
#else
    const bool ok = gmtime_r(&epoch, &timeInfo) != nullptr;
#endif
    if (!ok) {
        throw std::runtime_error("Failed to convert epoch to date");
    }

    char buffer[32];  // "YYYY-MM-DD\0" with headroom for GCC format-truncation warning
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                  timeInfo.tm_year + 1900,
                  timeInfo.tm_mon + 1,
                  timeInfo.tm_mday);
    return std::string(buffer);
}

bool isDateEarlierOrEqual(const std::string& dateA, const std::string& dateB) {
    const time_t epochA = convertDateToEpoch(dateA);
    const time_t epochB = convertDateToEpoch(dateB);
    return epochA <= epochB;
}

}  // namespace barcola
