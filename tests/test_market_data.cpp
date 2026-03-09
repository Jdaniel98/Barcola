#include <gtest/gtest.h>
#include "analysis/market_data_analysis.h"

using namespace barcola;

TEST(MarketDataTest, AveragePrice_KnownValues) {
    PriceHistory history("CONST");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/constant_price.csv");

    MarketDataAnalysis analysis(history);
    EXPECT_DOUBLE_EQ(analysis.calculateAveragePrice(), 100.0);
}

TEST(MarketDataTest, Volatility_ConstantPrice_IsZero) {
    PriceHistory history("CONST");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/constant_price.csv");

    MarketDataAnalysis analysis(history);
    EXPECT_DOUBLE_EQ(analysis.calculateVolatility(), 0.0);
}

TEST(MarketDataTest, Volatility_NonConstant_IsPositive) {
    PriceHistory history("AAPL");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");

    MarketDataAnalysis analysis(history);
    EXPECT_GT(analysis.calculateVolatility(), 0.0);
}

TEST(MarketDataTest, UpwardTrends_MonotonicUp) {
    PriceHistory history("UP");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    MarketDataAnalysis analysis(history);
    auto trends = analysis.getUpwardTrends();
    // All steps go up, so every point after the first is an upward trend
    EXPECT_EQ(trends.size(), history.dataPointsCount() - 1);
}

TEST(MarketDataTest, DownwardTrends_MonotonicDown) {
    PriceHistory history("DOWN");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_down.csv");

    MarketDataAnalysis analysis(history);
    auto trends = analysis.getDownwardTrends();
    EXPECT_EQ(trends.size(), history.dataPointsCount() - 1);
}

TEST(MarketDataTest, EmptyData_NoCrash) {
    PriceHistory history("EMPTY");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/empty.csv");

    MarketDataAnalysis analysis(history);
    EXPECT_DOUBLE_EQ(analysis.calculateAveragePrice(), 0.0);
    EXPECT_DOUBLE_EQ(analysis.calculateVolatility(), 0.0);
    EXPECT_TRUE(analysis.getUpwardTrends().empty());
}
