#include <gtest/gtest.h>
#include "backtest/strategy.h"
#include "mean_reversion_strategy.h"
#include "core/price_point.h"

using namespace barcola;

// Helper: build a vector of PricePoints from closing prices
static std::vector<PricePoint> makePrices(const std::vector<double>& closes) {
    std::vector<PricePoint> pts;
    pts.reserve(closes.size());
    for (size_t i = 0; i < closes.size(); ++i) {
        // Date doesn't matter for signal logic; use sequential dates
        std::string date = std::string("2023-01-") +
                           (i + 1 < 10 ? "0" : "") +
                           std::to_string(i + 1);
        pts.emplace_back(date, closes[i]);
    }
    return pts;
}

TEST(MeanReversionStrategyTest, Name_ContainsParameters) {
    MeanReversionStrategy strategy(20, 2.0, 0.0);
    const std::string n = strategy.name();
    EXPECT_NE(n.find("20"), std::string::npos);
    EXPECT_NE(n.find("2"), std::string::npos);
}

TEST(MeanReversionStrategyTest, InsufficientHistory_ReturnsHold) {
    MeanReversionStrategy strategy(10, 2.0, 0.0);
    auto prices = makePrices({100, 101, 102, 103, 104});

    // All indices < lookback(10) should return HOLD
    for (size_t i = 0; i < prices.size(); ++i) {
        EXPECT_EQ(strategy.evaluate(prices, i), Signal::HOLD);
    }
}

TEST(MeanReversionStrategyTest, FlatPrices_NeverTrades) {
    MeanReversionStrategy strategy(5, 2.0, 0.0);
    // 20 bars of constant price => z-score is always 0
    std::vector<double> closes(20, 100.0);
    auto prices = makePrices(closes);

    for (size_t i = 0; i < prices.size(); ++i) {
        EXPECT_EQ(strategy.evaluate(prices, i), Signal::HOLD);
    }
}

TEST(MeanReversionStrategyTest, SharpDrop_TriggersBuy) {
    MeanReversionStrategy strategy(5, 1.5, 0.0);

    // 5 bars of steady price, then a sharp drop
    std::vector<double> closes = {100, 100, 100, 100, 100, 90};
    auto prices = makePrices(closes);

    // At index 5 (enough lookback), price 90 is well below the 5-bar mean ~98.3
    // z-score should be negative and large
    Signal sig = strategy.evaluate(prices, 5);
    EXPECT_EQ(sig, Signal::BUY);
}

TEST(MeanReversionStrategyTest, BuyThenRevert_TriggersSell) {
    MeanReversionStrategy strategy(5, 1.5, 0.0);

    // Steady, then sharp drop (triggers BUY), then recovery (triggers SELL)
    std::vector<double> closes = {100, 100, 100, 100, 100, 85, 88, 92, 96, 100};
    auto prices = makePrices(closes);

    // Walk through the strategy
    bool boughtSomewhere = false;
    bool soldSomewhere = false;
    for (size_t i = 0; i < prices.size(); ++i) {
        Signal sig = strategy.evaluate(prices, i);
        if (sig == Signal::BUY) boughtSomewhere = true;
        if (sig == Signal::SELL) soldSomewhere = true;
    }

    EXPECT_TRUE(boughtSomewhere);
    EXPECT_TRUE(soldSomewhere);
}

TEST(MeanReversionStrategyTest, NoDuplicateBuys) {
    // Strategy should not issue BUY twice without a SELL in between
    MeanReversionStrategy strategy(5, 1.0, 0.5);

    // Drop that stays low
    std::vector<double> closes = {100, 100, 100, 100, 100, 85, 84, 83, 82, 81};
    auto prices = makePrices(closes);

    int buyCount = 0;
    for (size_t i = 0; i < prices.size(); ++i) {
        if (strategy.evaluate(prices, i) == Signal::BUY) {
            buyCount++;
        }
    }

    EXPECT_EQ(buyCount, 1);
}
