#include "core/time_utils.h"

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace barcola {

time_t getCurrentEpoch() {
    return time(nullptr);
}

time_t convertDateToEpoch(const std::string& date) {
    if (date.size() < 10 || date[4] != '-' || date[7] != '-') {
        throw std::invalid_argument("Date must be in YYYY-MM-DD format: " + date);
    }

    int year = 0, month = 0, day = 0;
    if (std::sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        throw std::invalid_argument("Failed to parse date: " + date);
    }

    if (month < 1 || month > 12 || day < 1 || day > 31) {
        throw std::invalid_argument("Invalid date components: " + date);
    }

    struct tm timeStruct = {};
    timeStruct.tm_year = year - 1900;
    timeStruct.tm_mon = month - 1;
    timeStruct.tm_mday = day;

    return timegm(&timeStruct);
}

std::string convertEpochToDate(time_t epoch) {
    struct tm* timeInfo = gmtime(&epoch);
    if (!timeInfo) {
        throw std::runtime_error("Failed to convert epoch to date");
    }

    char buffer[32];  // "YYYY-MM-DD\0" with headroom for GCC format-truncation warning
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                  timeInfo->tm_year + 1900,
                  timeInfo->tm_mon + 1,
                  timeInfo->tm_mday);
    return std::string(buffer);
}

bool isDateEarlierOrEqual(const std::string& dateA, const std::string& dateB) {
    const time_t epochA = convertDateToEpoch(dateA);
    const time_t epochB = convertDateToEpoch(dateB);
    return epochA <= epochB;
}

}  // namespace barcola
