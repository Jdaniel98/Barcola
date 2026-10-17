#include <gtest/gtest.h>

#include "backtest/backtest_engine.h"
#include "buy_and_hold_strategy.h"

using namespace barcola;

TEST(BuyAndHoldStrategyTest, BuysWhenFlat_HoldsWhenLong) {
    std::vector<PricePoint> bars = {PricePoint("2023-01-03", 10.0)};
    BuyAndHoldStrategy strategy;
    EXPECT_EQ(strategy.evaluate(BarView(bars, 1), Position::FLAT), Signal::BUY);
    EXPECT_EQ(strategy.evaluate(BarView(bars, 1), Position::LONG), Signal::HOLD);
}

TEST(BuyAndHoldStrategyTest, Backtest_TracksPriceFromFirstFill) {
    // Prices 10, 11, ..., 25 with open == close. BUY on bar 0 fills at bar 1's open (11);
    // the position is closed at the last close (25).
    PriceHistory data("UP");
    data.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    BacktestConfig config;
    config.initialCapital = 110000.0;
    config.commissionRate = 0.0;
    config.slippageBps = 0.0;
    config.riskFreeRate = 0.0;
    BuyAndHoldStrategy strategy;
    const BacktestResult result = BacktestEngine(config).run(strategy, data);

    ASSERT_EQ(result.totalTrades, 1u);
    EXPECT_DOUBLE_EQ(result.trades[0].shares, 10000.0);
    EXPECT_DOUBLE_EQ(result.trades[0].entryPrice, 11.0);
    EXPECT_DOUBLE_EQ(result.trades[0].exitPrice, 25.0);
    EXPECT_NEAR(result.totalReturn, 10000.0 * 14.0 / 110000.0, 1e-12);
}

TEST(BuyAndHoldStrategyTest, Name) {
    EXPECT_EQ(BuyAndHoldStrategy().name(), "BuyAndHold");
}
