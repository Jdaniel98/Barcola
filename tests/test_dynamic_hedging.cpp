#include <gtest/gtest.h>

#include "risk/dynamic_hedging.h"

using namespace barcola;

TEST(DynamicHedgingTest, InsufficientData_EmptyResult) {
    PriceHistory history("SINGLE");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/single_point.csv");

    DynamicHedging hedging(history);
    auto result = hedging.computeMovingAverages(20, 50);
    EXPECT_TRUE(result.shortMA.empty());
    EXPECT_TRUE(result.longMA.empty());
}

TEST(DynamicHedgingTest, ShortMA_KnownValues) {
    // Monotonic up: 10, 11, 12, ..., 25 (16 points)
    // Short MA(3) at index 0: (10+11+12)/3 = 11.0
    PriceHistory history("UP");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    DynamicHedging hedging(history);
    auto result = hedging.computeMovingAverages(3, 5);

    EXPECT_GT(result.shortMA.size(), 0u);
    EXPECT_NEAR(result.shortMA[0], 11.0, 1e-10);
}

TEST(DynamicHedgingTest, LongMA_CorrectWindowSize) {
    // Bug 3 regression test: longMA must use the full longWindow for calculation.
    // Monotonic up: 10, 11, 12, 13, 14, ... (16 points)
    // Long MA(5) at index 0: (10+11+12+13+14)/5 = 12.0
    PriceHistory history("UP");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    DynamicHedging hedging(history);
    auto result = hedging.computeMovingAverages(3, 5);

    EXPECT_GT(result.longMA.size(), 0u);
    EXPECT_NEAR(result.longMA[0], 12.0, 1e-10);
}

TEST(DynamicHedgingTest, ShortAndLongMA_DifferentSizes) {
    PriceHistory history("AAPL");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");

    DynamicHedging hedging(history);
    auto result = hedging.computeMovingAverages(5, 10);

    // shortMA should be longer than longMA (more valid windows)
    EXPECT_GT(result.shortMA.size(), result.longMA.size());
}

TEST(DynamicHedgingTest, ShortNotLessThanLong_Throws) {
    // Regression: short >= long wrapped an unsigned offset and read outside the array
    PriceHistory history("AAPL");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");

    DynamicHedging hedging(history);
    EXPECT_THROW((void)hedging.computeMovingAverages(10, 5), std::invalid_argument);
    EXPECT_THROW((void)hedging.computeMovingAverages(5, 5), std::invalid_argument);
    EXPECT_THROW((void)hedging.computeMovingAverages(0, 5), std::invalid_argument);
    EXPECT_THROW(hedging.performDynamicHedging(20, 10), std::invalid_argument);
}

TEST(DynamicHedgingTest, AlignedAverages_EndOnSameBar) {
    // longMA[0] ends at bar 4; the shortMA ending there is shortMA[5 - 3] = mean(12, 13, 14)
    PriceHistory history("UP");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    DynamicHedging hedging(history);
    auto result = hedging.computeMovingAverages(3, 5);
    ASSERT_EQ(result.shortMA.size(), 14u);
    ASSERT_EQ(result.longMA.size(), 12u);
    EXPECT_NEAR(result.shortMA[2], 13.0, 1e-10);
}
