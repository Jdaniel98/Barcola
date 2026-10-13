#include <gtest/gtest.h>
#include "analysis/market_data_analysis.h"

using namespace barcola;

static PriceHistory makeHistory(const std::vector<double>& closes) {
    const std::vector<std::string> dates = {"2023-01-02", "2023-01-03", "2023-01-04",
                                            "2023-01-05", "2023-01-06"};
    PriceHistory history("TEST");
    for (size_t i = 0; i < closes.size(); ++i) {
        history.addDataPoint(PricePoint(dates[i], closes[i]));
    }
    return history;
}

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

TEST(MarketDataTest, Volatility_KnownValue) {
    // Log returns ln(1.1) and ln(0.9); sample std dev x sqrt(252), computed independently
    const PriceHistory history = makeHistory({100.0, 110.0, 99.0});
    EXPECT_NEAR(MarketDataAnalysis(history).calculateVolatility(), 2.252522969955, 1e-9);
}

TEST(MarketDataTest, Volatility_IndependentOfPriceLevel) {
    // Regression: volatility was the std dev of price levels, so a 5x pricier stock with the
    // same percentage moves looked 5x as volatile
    const PriceHistory cheap = makeHistory({100.0, 103.0, 99.0, 104.0, 101.0});
    const PriceHistory pricey = makeHistory({500.0, 515.0, 495.0, 520.0, 505.0});
    EXPECT_NEAR(MarketDataAnalysis(cheap).calculateVolatility(),
                MarketDataAnalysis(pricey).calculateVolatility(), 1e-12);
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
