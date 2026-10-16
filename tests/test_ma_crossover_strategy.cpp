#include <gtest/gtest.h>
#include "backtest/backtest_engine.h"
#include "core/time_utils.h"
#include "ma_crossover_strategy.h"

using namespace barcola;

// One bar per day from 2023-01-01, with open == close
static std::vector<PricePoint> makePrices(const std::vector<double>& closes) {
    std::vector<PricePoint> points;
    const time_t start = convertDateToEpoch("2023-01-01");
    for (size_t i = 0; i < closes.size(); ++i) {
        points.emplace_back(start + static_cast<time_t>(i) * 86400, closes[i]);
    }
    return points;
}

TEST(MACrossoverStrategyTest, InsufficientHistory_Holds) {
    MovingAverageCrossoverStrategy strategy(2, 5);
    auto prices = makePrices({1, 2, 3, 4});
    EXPECT_EQ(strategy.evaluate(BarView(prices, prices.size()), Position::FLAT), Signal::HOLD);
}

TEST(MACrossoverStrategyTest, ShortAboveLong_BuysWhenFlat_HoldsWhenLong) {
    // Last 2 closes average 4.5; last 5 average 3
    MovingAverageCrossoverStrategy strategy(2, 5);
    auto prices = makePrices({1, 2, 3, 4, 5});
    const BarView bars(prices, prices.size());
    EXPECT_EQ(strategy.evaluate(bars, Position::FLAT), Signal::BUY);
    EXPECT_EQ(strategy.evaluate(bars, Position::LONG), Signal::HOLD);
}

TEST(MACrossoverStrategyTest, ShortBelowLong_SellsWhenLong_HoldsWhenFlat) {
    // Last 2 closes average 1.5; last 5 average 3
    MovingAverageCrossoverStrategy strategy(2, 5);
    auto prices = makePrices({5, 4, 3, 2, 1});
    const BarView bars(prices, prices.size());
    EXPECT_EQ(strategy.evaluate(bars, Position::LONG), Signal::SELL);
    EXPECT_EQ(strategy.evaluate(bars, Position::FLAT), Signal::HOLD);
}

TEST(MACrossoverStrategyTest, EqualAverages_Holds) {
    MovingAverageCrossoverStrategy strategy(2, 5);
    auto prices = makePrices({7, 7, 7, 7, 7});
    const BarView bars(prices, prices.size());
    EXPECT_EQ(strategy.evaluate(bars, Position::FLAT), Signal::HOLD);
    EXPECT_EQ(strategy.evaluate(bars, Position::LONG), Signal::HOLD);
}

TEST(MACrossoverStrategyTest, UsesOnlyTheLatestWindows) {
    // An early spike outside both windows must not matter: last 2 average 2.5,
    // last 3 average (1 + 2 + 3) / 3 = 2
    MovingAverageCrossoverStrategy strategy(2, 3);
    auto prices = makePrices({1000, 1, 2, 3});
    EXPECT_EQ(strategy.evaluate(BarView(prices, prices.size()), Position::FLAT), Signal::BUY);
}

TEST(MACrossoverStrategyTest, Backtest_FollowsTrendAndExits) {
    // Rise for 10 days, then fall for 10. MA(2) vs MA(4): long during the rise, out once the
    // short average drops below the long one.
    std::vector<double> closes;
    for (int i = 1; i <= 10; ++i) closes.push_back(100.0 + i);
    for (int i = 1; i <= 10; ++i) closes.push_back(110.0 - 2.0 * i);
    PriceHistory data("TEST");
    for (const auto& point : makePrices(closes)) {
        data.addDataPoint(point);
    }

    BacktestConfig config;
    config.commissionRate = 0.0;
    config.slippageBps = 0.0;
    config.riskFreeRate = 0.0;
    MovingAverageCrossoverStrategy strategy(2, 4);
    const BacktestResult result = BacktestEngine(config).run(strategy, data);

    ASSERT_EQ(result.totalTrades, 1u);
    // BUY on bar 3 (first with 4 bars) fills at bar 4's open (105)
    EXPECT_DOUBLE_EQ(result.trades[0].entryPrice, 105.0);
    // The exit fills before the end of the data, not at the final close
    EXPECT_NE(result.trades[0].exitDate, data.getDataPoints().back().getDateString());
}

TEST(MACrossoverStrategyTest, InvalidWindows_Throw) {
    EXPECT_THROW(MovingAverageCrossoverStrategy(0, 5), std::invalid_argument);
    EXPECT_THROW(MovingAverageCrossoverStrategy(5, 5), std::invalid_argument);
    EXPECT_THROW(MovingAverageCrossoverStrategy(10, 5), std::invalid_argument);
}

TEST(MACrossoverStrategyTest, Name_ContainsWindows) {
    EXPECT_EQ(MovingAverageCrossoverStrategy(20, 50).name(), "MACrossover(short=20, long=50)");
}
