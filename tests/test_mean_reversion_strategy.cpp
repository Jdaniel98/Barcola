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

// Helper: walk the bars like the engine does (with instant fills), so the position the
// strategy is told about follows its own signals.
static std::vector<Signal> walk(Strategy& strategy, const std::vector<PricePoint>& prices) {
    std::vector<Signal> signals;
    Position position = Position::FLAT;
    for (size_t i = 0; i < prices.size(); ++i) {
        const Signal signal = strategy.evaluate(BarView(prices, i + 1), position);
        if (signal == Signal::BUY && position == Position::FLAT) position = Position::LONG;
        if (signal == Signal::SELL && position == Position::LONG) position = Position::FLAT;
        signals.push_back(signal);
    }
    return signals;
}

TEST(MeanReversionStrategyTest, Name_ContainsParameters) {
    MeanReversionStrategy strategy(20, 2.0, 0.0);
    const std::string n = strategy.name();
    EXPECT_NE(n.find("20"), std::string::npos);
    EXPECT_NE(n.find('2'), std::string::npos);
}

TEST(MeanReversionStrategyTest, InsufficientHistory_ReturnsHold) {
    MeanReversionStrategy strategy(10, 2.0, 0.0);
    auto prices = makePrices({100, 101, 102, 103, 104});

    // Every bar has fewer than lookback(10) + 1 bars of history
    for (size_t i = 0; i < prices.size(); ++i) {
        EXPECT_EQ(strategy.evaluate(BarView(prices, i + 1), Position::FLAT), Signal::HOLD);
    }
}

TEST(MeanReversionStrategyTest, FlatPrices_NeverTrades) {
    MeanReversionStrategy strategy(5, 2.0, 0.0);
    // 20 bars of constant price => z-score is always 0
    std::vector<double> closes(20, 100.0);
    auto prices = makePrices(closes);

    for (Signal signal : walk(strategy, prices)) {
        EXPECT_EQ(signal, Signal::HOLD);
    }
}

TEST(MeanReversionStrategyTest, SharpDrop_TriggersBuy) {
    MeanReversionStrategy strategy(5, 1.5, 0.0);

    // 5 bars of steady price, then a sharp drop
    std::vector<double> closes = {100, 100, 100, 100, 100, 90};
    auto prices = makePrices(closes);

    // At the last bar, 90 is well below the 5-bar mean of 98
    EXPECT_EQ(strategy.evaluate(BarView(prices, prices.size()), Position::FLAT), Signal::BUY);
}

TEST(MeanReversionStrategyTest, SharpDrop_WhileLong_Holds) {
    MeanReversionStrategy strategy(5, 1.5, 0.0);
    auto prices = makePrices({100, 100, 100, 100, 100, 90});

    EXPECT_EQ(strategy.evaluate(BarView(prices, prices.size()), Position::LONG), Signal::HOLD);
}

TEST(MeanReversionStrategyTest, PositionComesFromCaller) {
    // Regression: the strategy used to remember its own BUY and refuse to buy again even when
    // the engine never filled it. It now has no hidden position state.
    MeanReversionStrategy strategy(5, 1.5, 0.0);
    auto prices = makePrices({100, 100, 100, 100, 100, 90});
    const BarView bars(prices, prices.size());

    EXPECT_EQ(strategy.evaluate(bars, Position::FLAT), Signal::BUY);
    EXPECT_EQ(strategy.evaluate(bars, Position::FLAT), Signal::BUY);
}

TEST(MeanReversionStrategyTest, BuyThenRevert_TriggersSell) {
    MeanReversionStrategy strategy(5, 1.5, 0.0);

    // Steady, then sharp drop (triggers BUY), then recovery (triggers SELL)
    std::vector<double> closes = {100, 100, 100, 100, 100, 85, 88, 92, 96, 100};
    auto prices = makePrices(closes);

    bool boughtSomewhere = false;
    bool soldSomewhere = false;
    for (Signal signal : walk(strategy, prices)) {
        if (signal == Signal::BUY) boughtSomewhere = true;
        if (signal == Signal::SELL) soldSomewhere = true;
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
    for (Signal signal : walk(strategy, prices)) {
        if (signal == Signal::BUY) buyCount++;
    }

    EXPECT_EQ(buyCount, 1);
}

TEST(MeanReversionStrategyTest, InvalidParameters_Throw) {
    EXPECT_THROW(MeanReversionStrategy(0, 2.0, 0.0), std::invalid_argument);
    EXPECT_THROW(MeanReversionStrategy(1, 2.0, 0.0), std::invalid_argument);
    EXPECT_THROW(MeanReversionStrategy(5, 0.0, 0.0), std::invalid_argument);
    EXPECT_THROW(MeanReversionStrategy(5, -1.0, 0.0), std::invalid_argument);
}

// --- Options: trend filter and time stop ---

TEST(MeanReversionStrategyTest, TrendFilter_BlocksDipInDowntrend) {
    // Steady fall from 100 to 71, then a sharp drop to 60: a dip, but below the 20-bar trend
    std::vector<double> closes;
    for (int i = 0; i < 30; ++i) closes.push_back(100.0 - i);
    closes.push_back(60.0);
    auto prices = makePrices(closes);
    const BarView bars(prices, prices.size());

    MeanReversionStrategy unfiltered(5, 1.5, 0.0);
    MeanReversionStrategy filtered(5, 1.5, 0.0, {20, 0});
    EXPECT_EQ(unfiltered.evaluate(bars, Position::FLAT), Signal::BUY);
    EXPECT_EQ(filtered.evaluate(bars, Position::FLAT), Signal::HOLD);
}

TEST(MeanReversionStrategyTest, TrendFilter_AllowsDipInUptrend) {
    // Steady rise from 100 to 129, then a drop to 124: still above the 20-bar trend
    std::vector<double> closes;
    for (int i = 0; i < 30; ++i) closes.push_back(100.0 + i);
    closes.push_back(124.0);
    auto prices = makePrices(closes);

    MeanReversionStrategy filtered(5, 1.5, 0.0, {20, 0});
    EXPECT_EQ(filtered.evaluate(BarView(prices, prices.size()), Position::FLAT), Signal::BUY);
}

TEST(MeanReversionStrategyTest, TrendFilter_WaitsForFullTrendWindow) {
    // Same dip as SharpDrop_TriggersBuy, but too few bars to compute a 20-bar trend
    auto prices = makePrices({100, 100, 100, 100, 100, 90});
    MeanReversionStrategy filtered(5, 1.5, 0.0, {20, 0});
    EXPECT_EQ(filtered.evaluate(BarView(prices, prices.size()), Position::FLAT), Signal::HOLD);
}

// Ten bars at 100, a drop to 85 (BUY on bar 10), then the price stays at 84 and never
// reverts, so only a time stop can sell
static std::vector<double> dropWithoutReversion() {
    std::vector<double> closes(10, 100.0);
    closes.push_back(85.0);
    for (int i = 0; i < 5; ++i) closes.push_back(84.0);
    return closes;
}

TEST(MeanReversionStrategyTest, TimeStop_SellsAfterMaxHoldingBars) {
    auto prices = makePrices(dropWithoutReversion());
    MeanReversionStrategy strategy(5, 1.5, 0.0, {0, 3});
    const auto signals = walk(strategy, prices);

    // BUY on bar 10; long from bar 11; three bars later (bar 14) the time stop sells
    ASSERT_EQ(signals[10], Signal::BUY);
    for (size_t i = 11; i < 14; ++i) {
        EXPECT_EQ(signals[i], Signal::HOLD) << "bar " << i;
    }
    EXPECT_EQ(signals[14], Signal::SELL);
}

TEST(MeanReversionStrategyTest, NoTimeStop_HoldsWhilePriceStaysLow) {
    auto prices = makePrices(dropWithoutReversion());
    MeanReversionStrategy strategy(5, 1.5, 0.0);
    for (Signal signal : walk(strategy, prices)) {
        EXPECT_NE(signal, Signal::SELL);
    }
}

TEST(MeanReversionStrategyTest, TimeStop_ResetStartsTheClockAgain) {
    auto prices = makePrices(dropWithoutReversion());
    MeanReversionStrategy strategy(5, 1.5, 0.0, {0, 3});
    // Long from bar 11: by bar 14 the position is three bars old
    for (size_t i = 11; i <= 13; ++i) {
        (void)strategy.evaluate(BarView(prices, i + 1), Position::LONG);
    }
    strategy.reset();
    // After a reset, bar 14 is the first bar seen long, so no time stop yet
    EXPECT_EQ(strategy.evaluate(BarView(prices, 15), Position::LONG), Signal::HOLD);
}

TEST(MeanReversionStrategyTest, Options_InName) {
    EXPECT_EQ(MeanReversionStrategy(20, 2.0, 0.5, {200, 21}).name(),
              "MeanReversion(lookback=20, entryZ=2, exitZ=0.5, trend=200, maxHold=21)");
    EXPECT_EQ(MeanReversionStrategy(20, 2.0, 0.5).name(),
              "MeanReversion(lookback=20, entryZ=2, exitZ=0.5)");
}

TEST(MeanReversionStrategyTest, TrendWindowOfOne_Throws) {
    EXPECT_THROW(MeanReversionStrategy(5, 1.5, 0.0, {1, 0}), std::invalid_argument);
}
