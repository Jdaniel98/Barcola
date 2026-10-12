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
    EXPECT_NE(n.find("2"), std::string::npos);
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
