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
