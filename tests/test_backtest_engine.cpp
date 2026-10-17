#include <gtest/gtest.h>

#include <cmath>

#include "backtest/backtest_engine.h"
#include "mean_reversion_strategy.h"

using namespace barcola;

static std::string dataDir() {
    return std::string(BARCOLA_TEST_DATA_DIR);
}

// Config with no costs and no interest on cash, so expected values can be worked out by hand.
static BacktestConfig frictionlessConfig() {
    BacktestConfig config;
    config.initialCapital = 100000.0;
    config.commissionRate = 0.0;
    config.slippageBps = 0.0;
    config.riskFreeRate = 0.0;
    return config;
}

// A trivial strategy that always holds — never trades
class AlwaysHoldStrategy : public Strategy {
public:
    Signal evaluate(const BarView&, Position) override { return Signal::HOLD; }
    [[nodiscard]] std::string name() const override { return "AlwaysHold"; }
};

// A strategy that signals BUY on bar 1 and SELL on bar 3
class BuySellFixedStrategy : public Strategy {
public:
    Signal evaluate(const BarView& bars, Position) override {
        const size_t idx = bars.size() - 1;
        if (idx == 1)
            return Signal::BUY;
        if (idx == 3)
            return Signal::SELL;
        return Signal::HOLD;
    }
    [[nodiscard]] std::string name() const override { return "BuySellFixed"; }
};

// Always wants to be long; records what the engine tells it on every bar
class AlwaysBuyStrategy : public Strategy {
public:
    std::vector<Position> positionsSeen;
    std::vector<size_t> barsSeen;

    Signal evaluate(const BarView& bars, Position position) override {
        positionsSeen.push_back(position);
        barsSeen.push_back(bars.size());
        return Signal::BUY;
    }
    [[nodiscard]] std::string name() const override { return "AlwaysBuy"; }
};

// Buys once per run and relies on reset() to do it again on the next run
class BuyOnceStrategy : public Strategy {
public:
    int resets = 0;

    void reset() override {
        bought_ = false;
        ++resets;
    }
    Signal evaluate(const BarView& bars, Position) override {
        if (!bought_) {
            bought_ = true;
            return Signal::BUY;
        }
        return bars.size() == 4 ? Signal::SELL : Signal::HOLD;
    }
    [[nodiscard]] std::string name() const override { return "BuyOnce"; }

private:
    bool bought_ = false;
};

// Signals BUY only on the final bar
class BuyOnLastBarStrategy : public Strategy {
public:
    explicit BuyOnLastBarStrategy(size_t totalBars) : totalBars_(totalBars) {}
    Signal evaluate(const BarView& bars, Position) override {
        return bars.size() == totalBars_ ? Signal::BUY : Signal::HOLD;
    }
    [[nodiscard]] std::string name() const override { return "BuyOnLastBar"; }

private:
    size_t totalBars_;
};

TEST(BacktestEngineTest, NoTrades_EquityFlat) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");

    BacktestEngine engine(frictionlessConfig());

    AlwaysHoldStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    EXPECT_EQ(result.totalTrades, 0u);
    EXPECT_NEAR(result.totalReturn, 0.0, 1e-10);
    EXPECT_EQ(result.equityCurve.size(), data.dataPointsCount());
    // All equity curve values should be initial capital
    for (double eq : result.equityCurve) {
        EXPECT_DOUBLE_EQ(eq, 100000.0);
    }
}

TEST(BacktestEngineTest, IdleCash_EarnsRiskFreeRate) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");

    BacktestConfig config = frictionlessConfig();
    config.riskFreeRate = 0.05;
    BacktestEngine engine(config);

    AlwaysHoldStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    const double periods = static_cast<double>(data.dataPointsCount() - 1);
    const double expected = 100000.0 * std::pow(1.0 + 0.05 / 252.0, periods);
    EXPECT_NEAR(result.equityCurve.back(), expected, 1e-6);
    // Earning exactly the hurdle rate is not excess return
    EXPECT_NEAR(result.sharpeRatio, 0.0, 1e-9);
}

TEST(BacktestEngineTest, SingleRoundTrip_FillsAtNextOpen) {
    // Prices 10, 11, 12, 13, 14, ... with open == close on every bar
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestEngine engine(frictionlessConfig());

    BuySellFixedStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    ASSERT_EQ(result.totalTrades, 1u);
    EXPECT_EQ(result.trades[0].symbol, "TEST");
    // BUY decided on bar 1's close fills at bar 2's open; SELL on bar 3 fills at bar 4's open
    EXPECT_DOUBLE_EQ(result.trades[0].entryPrice, 12.0);
    EXPECT_DOUBLE_EQ(result.trades[0].exitPrice, 14.0);
    EXPECT_EQ(result.trades[0].entryDate, "2023-01-05");
    EXPECT_EQ(result.trades[0].exitDate, "2023-01-09");
    EXPECT_GT(result.trades[0].pnl, 0.0);
}

TEST(BacktestEngineTest, SameCloseFill_UsesSignalBarClose) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestConfig config = frictionlessConfig();
    config.fillTiming = FillTiming::SAME_CLOSE;
    BacktestEngine engine(config);

    BuySellFixedStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    ASSERT_EQ(result.totalTrades, 1u);
    EXPECT_DOUBLE_EQ(result.trades[0].entryPrice, 11.0);
    EXPECT_DOUBLE_EQ(result.trades[0].exitPrice, 13.0);
}

TEST(BacktestEngineTest, DefaultSizing_InvestsAllAvailableCash) {
    // Regression: the old riskPerTrade=0.02 default put only 2% of capital into each trade
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestEngine engine(frictionlessConfig());
    BuySellFixedStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    // floor(100000 / 12) = 8333 shares bought at 12, sold at 14
    ASSERT_EQ(result.totalTrades, 1u);
    EXPECT_DOUBLE_EQ(result.trades[0].shares, 8333.0);
    EXPECT_NEAR(result.totalReturn, 8333.0 * 2.0 / 100000.0, 1e-12);
}

TEST(BacktestEngineTest, PositionFraction_ScalesTradeSize) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestConfig config = frictionlessConfig();
    config.positionFraction = 0.5;
    BacktestEngine engine(config);
    BuySellFixedStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    ASSERT_EQ(result.totalTrades, 1u);
    EXPECT_DOUBLE_EQ(result.trades[0].shares, 4166.0);  // floor(50000 / 12)
}

TEST(BacktestEngineTest, Commission_IncludedInSizing) {
    // Regression: sizing ignored commission, so a full-cash entry overdrew the account
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestConfig config = frictionlessConfig();
    config.commissionRate = 0.01;
    BacktestEngine engine(config);
    BuySellFixedStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    ASSERT_EQ(result.totalTrades, 1u);
    const Trade& trade = result.trades[0];
    EXPECT_LE(trade.shares * trade.entryPrice * 1.01, 100000.0);
    EXPECT_DOUBLE_EQ(trade.shares, 8250.0);  // floor(100000 / (12 * 1.01))
}

TEST(BacktestEngineTest, Commission_ReducesReturns) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestConfig configZero = frictionlessConfig();
    configZero.positionFraction = 0.5;
    BacktestEngine engineZero(configZero);

    BacktestConfig configComm = configZero;
    configComm.commissionRate = 0.01;  // 1%
    BacktestEngine engineComm(configComm);

    BuySellFixedStrategy s1;
    BuySellFixedStrategy s2;
    const auto resultZero = engineZero.run(s1, data);
    const auto resultComm = engineComm.run(s2, data);

    // Commission should reduce final equity
    EXPECT_GT(resultZero.equityCurve.back(), resultComm.equityCurve.back());
}

TEST(BacktestEngineTest, Slippage_ReducesReturns) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestConfig configZero = frictionlessConfig();
    configZero.positionFraction = 0.5;
    BacktestEngine engineZero(configZero);

    BacktestConfig configSlip = configZero;
    configSlip.slippageBps = 50.0;  // 50 bps
    BacktestEngine engineSlip(configSlip);

    BuySellFixedStrategy s1;
    BuySellFixedStrategy s2;
    const auto resultZero = engineZero.run(s1, data);
    const auto resultSlip = engineSlip.run(s2, data);

    EXPECT_GT(resultZero.equityCurve.back(), resultSlip.equityCurve.back());
}

TEST(BacktestEngineTest, Strategy_SeesOnlyBarsUpToCurrent) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestEngine engine(frictionlessConfig());
    AlwaysBuyStrategy strategy;
    (void)engine.run(strategy, data);

    ASSERT_EQ(strategy.barsSeen.size(), data.dataPointsCount());
    for (size_t i = 0; i < strategy.barsSeen.size(); ++i) {
        EXPECT_EQ(strategy.barsSeen[i], i + 1);
    }
}

TEST(BacktestEngineTest, SignalOnLastBar_IsNotFilled) {
    // With next-open fills there is no bar left to fill an order decided on the last close
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestEngine engine(frictionlessConfig());
    BuyOnLastBarStrategy strategy(data.dataPointsCount());
    const BacktestResult result = engine.run(strategy, data);

    EXPECT_EQ(result.totalTrades, 0u);
}

TEST(BacktestEngineTest, PositionPassedToStrategy_MatchesEngine) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestEngine engine(frictionlessConfig());
    AlwaysBuyStrategy strategy;
    (void)engine.run(strategy, data);

    // BUY on bar 0 fills at bar 1's open, so the strategy is told it is long from bar 1 on
    ASSERT_GE(strategy.positionsSeen.size(), 2u);
    EXPECT_EQ(strategy.positionsSeen[0], Position::FLAT);
    for (size_t i = 1; i < strategy.positionsSeen.size(); ++i) {
        EXPECT_EQ(strategy.positionsSeen[i], Position::LONG);
    }
}

TEST(BacktestEngineTest, UnaffordableEntry_StrategyStaysFlat) {
    // Regression: the strategy used to assume it was long even when the engine bought nothing
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestConfig config = frictionlessConfig();
    config.initialCapital = 5.0;  // cheaper than one share
    BacktestEngine engine(config);
    AlwaysBuyStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    EXPECT_EQ(result.totalTrades, 0u);
    for (Position position : strategy.positionsSeen) {
        EXPECT_EQ(position, Position::FLAT);
    }
}

TEST(BacktestEngineTest, ReusedStrategy_ResetBetweenRuns) {
    // Regression: strategy state leaked from one run into the next
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestEngine engine(frictionlessConfig());
    BuyOnceStrategy strategy;
    const BacktestResult first = engine.run(strategy, data);
    const BacktestResult second = engine.run(strategy, data);

    EXPECT_EQ(strategy.resets, 2);
    ASSERT_EQ(first.totalTrades, 1u);
    ASSERT_EQ(second.totalTrades, 1u);
    EXPECT_DOUBLE_EQ(first.trades[0].pnl, second.trades[0].pnl);
    EXPECT_EQ(first.equityCurve, second.equityCurve);
}

TEST(BacktestEngineTest, EmptyData_Throws) {
    PriceHistory data("EMPTY");
    data.loadFromCsv(dataDir() + "/empty.csv");

    BacktestEngine engine(BacktestConfig{});
    AlwaysHoldStrategy strategy;

    EXPECT_THROW((void)engine.run(strategy, data), std::invalid_argument);
}

TEST(BacktestEngineTest, InvalidConfig_Throws) {
    BacktestConfig zeroFraction;
    zeroFraction.positionFraction = 0.0;
    EXPECT_THROW(BacktestEngine{zeroFraction}, std::invalid_argument);

    BacktestConfig leveraged;
    leveraged.positionFraction = 1.5;
    EXPECT_THROW(BacktestEngine{leveraged}, std::invalid_argument);

    BacktestConfig noCapital;
    noCapital.initialCapital = 0.0;
    EXPECT_THROW(BacktestEngine{noCapital}, std::invalid_argument);

    BacktestConfig negativeCommission;
    negativeCommission.commissionRate = -0.01;
    EXPECT_THROW(BacktestEngine{negativeCommission}, std::invalid_argument);
}

TEST(BacktestEngineTest, MetricsArePopulated) {
    PriceHistory data("AAPL");
    data.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");

    BacktestConfig config;
    config.initialCapital = 100000.0;
    BacktestEngine engine(config);

    // Use mean reversion on the sample data — may or may not trade,
    // but metrics should be computed without crashing
    MeanReversionStrategy strategy(5, 1.5, 0.0);
    const BacktestResult result = engine.run(strategy, data);

    // Equity curve should match data length
    EXPECT_EQ(result.equityCurve.size(), data.dataPointsCount());
    // Metrics should be finite
    EXPECT_FALSE(std::isnan(result.sharpeRatio));
    EXPECT_FALSE(std::isnan(result.maxDrawdown));
    EXPECT_FALSE(std::isnan(result.sortinoRatio));
    EXPECT_FALSE(std::isnan(result.annualizedReturn));
    EXPECT_GE(result.maxDrawdown, 0.0);
    EXPECT_LE(result.maxDrawdown, 1.0);
}

TEST(BacktestEngineTest, Exposure_FractionOfBarsHeld) {
    // AlwaysBuy is flat on bar 0 and long from bar 1 to the end: 15 of 16 bars
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestEngine engine(frictionlessConfig());
    AlwaysBuyStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    EXPECT_DOUBLE_EQ(result.exposure, 15.0 / 16.0);
}

TEST(BacktestEngineTest, Exposure_ZeroWithoutTrades) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestEngine engine(frictionlessConfig());
    AlwaysHoldStrategy strategy;
    EXPECT_DOUBLE_EQ(engine.run(strategy, data).exposure, 0.0);
}

TEST(BacktestEngineTest, Warmup_StrategySeesHistoryButTradingStartsLater) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestConfig config = frictionlessConfig();
    config.warmupBars = 3;
    BacktestEngine engine(config);
    AlwaysBuyStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    // First evaluation is on bar 3 with bars 0-3 visible
    ASSERT_FALSE(strategy.barsSeen.empty());
    EXPECT_EQ(strategy.barsSeen.front(), 4u);
    EXPECT_EQ(strategy.barsSeen.size(), 13u);
    EXPECT_EQ(result.equityCurve.size(), 13u);
    EXPECT_DOUBLE_EQ(result.equityCurve.front(), 100000.0);

    // BUY on bar 3 fills at bar 4's open (14)
    ASSERT_EQ(result.totalTrades, 1u);
    EXPECT_DOUBLE_EQ(result.trades[0].entryPrice, 14.0);
    EXPECT_EQ(result.trades[0].entryDate, "2023-01-09");
}

TEST(BacktestEngineTest, Warmup_LeavingNoBars_Throws) {
    PriceHistory data("TEST");
    data.loadFromCsv(dataDir() + "/monotonic_up.csv");

    BacktestConfig config = frictionlessConfig();
    config.warmupBars = data.dataPointsCount();
    BacktestEngine engine(config);
    AlwaysHoldStrategy strategy;
    EXPECT_THROW((void)engine.run(strategy, data), std::invalid_argument);
}
