#include <gtest/gtest.h>
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

TEST(BollingerTest, PriceHistoryMethod_ReturnsBothBands) {
    PriceHistory history("AAPL");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");
    auto [upper, lower] = history.calculateBollingerBands(10, 2.0);
    EXPECT_EQ(upper.size(), lower.size());
    EXPECT_GT(upper.size(), 0u);
}
