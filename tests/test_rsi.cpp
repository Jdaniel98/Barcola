#include <gtest/gtest.h>
#include "indicators/rsi.h"
#include "core/price_history.h"

using namespace barcola;

static std::vector<double> loadCloses(const std::string& file) {
    PriceHistory history("TEST");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/" + file);
    std::vector<double> prices;
    for (const auto& p : history.getDataPoints()) {
        prices.push_back(p.getClosing());
    }
    return prices;
}

TEST(RSITest, AllGains_Returns100) {
    // Monotonically increasing: RSI should be 100 (Bug 1 regression test)
    const auto rsi = calculateRSI(loadCloses("monotonic_up.csv"), 14);
    ASSERT_TRUE(rsi.has_value());
    EXPECT_DOUBLE_EQ(*rsi, 100.0);
}

TEST(RSITest, AllLosses_Returns0) {
    const auto rsi = calculateRSI(loadCloses("monotonic_down.csv"), 14);
    ASSERT_TRUE(rsi.has_value());
    EXPECT_DOUBLE_EQ(*rsi, 0.0);
}

TEST(RSITest, FlatPrices_Returns50) {
    // No gains and no losses is neutral, not overbought
    const auto rsi = calculateRSI(loadCloses("constant_price.csv"), 5);
    ASSERT_TRUE(rsi.has_value());
    EXPECT_DOUBLE_EQ(*rsi, 50.0);
}

TEST(RSITest, InsufficientData_ReturnsNullopt) {
    // Used to return 0.0, which reads as "maximally oversold"
    std::vector<double> prices = {10.0, 20.0, 30.0};
    EXPECT_FALSE(calculateRSI(prices, 14).has_value());
}

TEST(RSITest, ZeroPeriod_Throws) {
    std::vector<double> prices = {10.0, 20.0, 30.0};
    EXPECT_THROW((void)calculateRSI(prices, 0), std::invalid_argument);
}

TEST(RSITest, KnownValues) {
    // Hand-calculated: prices 100, 102, 101, 103, 104, 102 (period=5)
    // Changes: +2, -1, +2, +1, -2
    // avgGain = (2+2+1)/5 = 1.0, avgLoss = (1+2)/5 = 0.6
    // RS = 1.0/0.6 = 1.667, RSI = 100 - 100/(1+1.667) = 62.5
    std::vector<double> prices = {100.0, 102.0, 101.0, 103.0, 104.0, 102.0};
    const auto rsi = calculateRSI(prices, 5);
    ASSERT_TRUE(rsi.has_value());
    EXPECT_NEAR(*rsi, 62.5, 1e-10);
}

TEST(RSITest, WilderSmoothing_KnownValue) {
    // Seed with the period-5 averages above (gain 1.0, loss 0.6), then one more change of +3:
    // avgGain = (1.0 * 4 + 3) / 5 = 1.4, avgLoss = (0.6 * 4 + 0) / 5 = 0.48
    // RSI = 100 - 100 / (1 + 1.4 / 0.48)
    std::vector<double> prices = {100.0, 102.0, 101.0, 103.0, 104.0, 102.0, 105.0};
    const auto rsi = calculateRSI(prices, 5);
    ASSERT_TRUE(rsi.has_value());
    EXPECT_NEAR(*rsi, 100.0 - 100.0 / (1.0 + 1.4 / 0.48), 1e-10);
}

TEST(RSITest, UsesLatestPrices_NotFirstWindow) {
    // Regression: RSI only looked at the first `period` changes. Prices rise for 15 bars and
    // then fall for 15; the old code returned 100, the latest RSI is well below 50.
    std::vector<double> prices;
    for (int i = 0; i <= 15; ++i) prices.push_back(100.0 + i);
    for (int i = 1; i <= 15; ++i) prices.push_back(115.0 - i);

    const auto rsi = calculateRSI(prices, 14);
    ASSERT_TRUE(rsi.has_value());
    EXPECT_NEAR(*rsi, 32.902565, 1e-6);
}

TEST(RSITest, AaplSample_KnownValue) {
    // Wilder RSI(14) over all 30 closes, computed independently
    const auto rsi = calculateRSI(loadCloses("AAPL_2023_sample.csv"), 14);
    ASSERT_TRUE(rsi.has_value());
    EXPECT_NEAR(*rsi, 71.8160315173, 1e-8);
}
