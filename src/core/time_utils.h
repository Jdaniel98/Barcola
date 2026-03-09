#pragma once

#include <ctime>
#include <string>

namespace barcola {

[[nodiscard]] time_t getCurrentEpoch();
[[nodiscard]] time_t convertDateToEpoch(const std::string& date);
[[nodiscard]] std::string convertEpochToDate(time_t epoch);
[[nodiscard]] bool isDateEarlierOrEqual(const std::string& dateA, const std::string& dateB);

}  // namespace barcola
