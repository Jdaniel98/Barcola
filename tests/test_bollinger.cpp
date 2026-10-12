#include <gtest/gtest.h>
#include <cmath>
#include "indicators/bollinger_bands.h"
#include "core/price_history.h"

using namespace barcola;

TEST(BollingerTest, ConstantPrice_BandsEqual) {
    // Constant prices = zero std dev, so upper == lower == price
    std::vector<double> prices(10, 100.0);
    auto [upper, lower] = calculateBollingerBands(prices, 5, 2.0);
    EXPECT_EQ(upper.size(), 6u);  // 10 - 5 + 1 = 6
    for (size_t i = 0; i < upper.size(); ++i) {
        EXPECT_DOUBLE_EQ(upper[i], 100.0);
        EXPECT_DOUBLE_EQ(lower[i], 100.0);
    }
}

TEST(BollingerTest, InsufficientData_ReturnsEmpty) {
    std::vector<double> prices = {100.0, 110.0};
    auto [upper, lower] = calculateBollingerBands(prices, 5, 2.0);
    EXPECT_TRUE(upper.empty());
    EXPECT_TRUE(lower.empty());
}

TEST(BollingerTest, UpperAboveLower) {
    // Non-constant prices should produce upper > lower
    PriceHistory history("AAPL");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");

    const auto& points = history.getDataPoints();
    std::vector<double> prices;
    for (const auto& p : points) {
        prices.push_back(p.getClosing());
    }

    auto [upper, lower] = calculateBollingerBands(prices, 10, 2.0);
    EXPECT_GT(upper.size(), 0u);
    for (size_t i = 0; i < upper.size(); ++i) {
        EXPECT_GE(upper[i], lower[i]);
    }
}

TEST(BollingerTest, ReturnsBothBands) {
    PriceHistory history("AAPL");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");
    auto [upper, lower] = calculateBollingerBands(history.getClosingPrices(), 10, 2.0);
    EXPECT_EQ(upper.size(), lower.size());
    EXPECT_EQ(upper.size(), 21u);  // 30 - 10 + 1
}

TEST(BollingerTest, KnownValues) {
    // Window {1, 2, 3}: mean 2, population std dev sqrt(2/3)
    auto [upper, lower] = calculateBollingerBands({1.0, 2.0, 3.0}, 3, 2.0);
    ASSERT_EQ(upper.size(), 1u);
    EXPECT_NEAR(upper[0], 2.0 + 2.0 * std::sqrt(2.0 / 3.0), 1e-12);
    EXPECT_NEAR(lower[0], 2.0 - 2.0 * std::sqrt(2.0 / 3.0), 1e-12);
}

TEST(BollingerTest, ZeroPeriod_Throws) {
    EXPECT_THROW((void)calculateBollingerBands({1.0, 2.0}, 0, 2.0), std::invalid_argument);
}
