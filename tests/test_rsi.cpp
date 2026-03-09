#include <gtest/gtest.h>
#include "indicators/rsi.h"
#include "core/price_history.h"

using namespace barcola;

TEST(RSITest, AllGains_Returns100) {
    // Monotonically increasing: RSI should be 100 (Bug 1 regression test)
    PriceHistory history("UP");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    const auto& points = history.getDataPoints();
    std::vector<double> prices;
    for (const auto& p : points) {
        prices.push_back(p.getClosing());
    }

    double rsi = calculateRSI(prices, 14);
    EXPECT_DOUBLE_EQ(rsi, 100.0);
}

TEST(RSITest, AllLosses_Returns0) {
    PriceHistory history("DOWN");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_down.csv");

    const auto& points = history.getDataPoints();
    std::vector<double> prices;
    for (const auto& p : points) {
        prices.push_back(p.getClosing());
    }

    double rsi = calculateRSI(prices, 14);
    EXPECT_DOUBLE_EQ(rsi, 0.0);
}

TEST(RSITest, InsufficientData_Returns0) {
    std::vector<double> prices = {10.0, 20.0, 30.0};
    EXPECT_DOUBLE_EQ(calculateRSI(prices, 14), 0.0);
}

TEST(RSITest, KnownValues) {
    // Hand-calculated: prices 100, 102, 101, 103, 104, 102 (period=5)
    // Changes: +2, -1, +2, +1, -2
    // avgGain = (2+2+1)/5 = 1.0, avgLoss = (1+2)/5 = 0.6
    // RS = 1.0/0.6 = 1.667, RSI = 100 - 100/(1+1.667) = 62.5
    std::vector<double> prices = {100.0, 102.0, 101.0, 103.0, 104.0, 102.0};
    double rsi = calculateRSI(prices, 5);
    EXPECT_NEAR(rsi, 62.5, 0.1);
}
