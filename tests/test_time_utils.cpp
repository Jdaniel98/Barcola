#include <gtest/gtest.h>
#include "core/time_utils.h"

using namespace barcola;

TEST(TimeUtilsTest, ConvertDateToEpoch_ValidDate) {
    time_t epoch = convertDateToEpoch("2023-01-15");
    EXPECT_GT(epoch, 0);
}

TEST(TimeUtilsTest, RoundTrip_DateToEpochAndBack) {
    std::string original = "2023-06-15";
    time_t epoch = convertDateToEpoch(original);
    std::string result = convertEpochToDate(epoch);
    EXPECT_EQ(result, original);
}

TEST(TimeUtilsTest, RoundTrip_MultipleFormats) {
    std::vector<std::string> dates = {"2020-01-01", "2023-12-31", "2000-06-15"};
    for (const auto& date : dates) {
        EXPECT_EQ(convertEpochToDate(convertDateToEpoch(date)), date);
    }
}

TEST(TimeUtilsTest, IsDateEarlierOrEqual_Earlier) {
    EXPECT_TRUE(isDateEarlierOrEqual("2023-01-01", "2023-06-15"));
}

TEST(TimeUtilsTest, IsDateEarlierOrEqual_Equal) {
    EXPECT_TRUE(isDateEarlierOrEqual("2023-06-15", "2023-06-15"));
}

TEST(TimeUtilsTest, IsDateEarlierOrEqual_Later) {
    EXPECT_FALSE(isDateEarlierOrEqual("2023-12-31", "2023-01-01"));
}

TEST(TimeUtilsTest, InvalidDateFormat_Throws) {
    EXPECT_THROW(convertDateToEpoch("not-a-date"), std::invalid_argument);
    EXPECT_THROW(convertDateToEpoch("2023/01/15"), std::invalid_argument);
    EXPECT_THROW(convertDateToEpoch(""), std::invalid_argument);
}

TEST(TimeUtilsTest, GetCurrentEpoch_ReturnsPositive) {
    EXPECT_GT(getCurrentEpoch(), 0);
}
