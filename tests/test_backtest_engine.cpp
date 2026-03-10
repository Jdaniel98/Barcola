#include <gtest/gtest.h>
#include <cmath>
#include "backtest/backtest_engine.h"
#include "mean_reversion_strategy.h"

using namespace barcola;

// A trivial strategy that always holds — never trades
class AlwaysHoldStrategy : public Strategy {
public:
    Signal evaluate(const std::vector<PricePoint>&, size_t) override {
        return Signal::HOLD;
    }
    [[nodiscard]] std::string name() const override { return "AlwaysHold"; }
};

// A strategy that buys on bar 1 and sells on bar 3
class BuySellFixedStrategy : public Strategy {
public:
    Signal evaluate(const std::vector<PricePoint>&, size_t idx) override {
        if (idx == 1) return Signal::BUY;
        if (idx == 3) return Signal::SELL;
        return Signal::HOLD;
    }
    [[nodiscard]] std::string name() const override { return "BuySellFixed"; }
};

TEST(BacktestEngineTest, NoTrades_EquityFlat) {
    PriceHistory data("TEST");
    data.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");

    BacktestConfig config;
    config.initialCapital = 100000.0;
    BacktestEngine engine(config);

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

TEST(BacktestEngineTest, SingleRoundTrip_ProducesOneTrade) {
    PriceHistory data("TEST");
    data.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    BacktestConfig config;
    config.initialCapital = 100000.0;
    config.commissionRate = 0.0;
    config.slippageBps = 0.0;
    config.riskPerTrade = 1.0;  // use full capital
    BacktestEngine engine(config);

    BuySellFixedStrategy strategy;
    const BacktestResult result = engine.run(strategy, data);

    EXPECT_EQ(result.totalTrades, 1u);
    EXPECT_EQ(result.trades[0].symbol, "TEST");
    // Bought at bar 1, sold at bar 3 — prices are monotonically up, so profit
    EXPECT_GT(result.trades[0].pnl, 0.0);
    EXPECT_GT(result.trades[0].returnPct, 0.0);
}

TEST(BacktestEngineTest, Commission_ReducesReturns) {
    PriceHistory data("TEST");
    data.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    // Run with zero commission
    BacktestConfig configZero;
    configZero.initialCapital = 100000.0;
    configZero.commissionRate = 0.0;
    configZero.slippageBps = 0.0;
    configZero.riskPerTrade = 0.5;
    BacktestEngine engineZero(configZero);

    // Run with commission
    BacktestConfig configComm;
    configComm.initialCapital = 100000.0;
    configComm.commissionRate = 0.01;  // 1%
    configComm.slippageBps = 0.0;
    configComm.riskPerTrade = 0.5;
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
    data.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    BacktestConfig configZero;
    configZero.initialCapital = 100000.0;
    configZero.commissionRate = 0.0;
    configZero.slippageBps = 0.0;
    configZero.riskPerTrade = 0.5;
    BacktestEngine engineZero(configZero);

    BacktestConfig configSlip;
    configSlip.initialCapital = 100000.0;
    configSlip.commissionRate = 0.0;
    configSlip.slippageBps = 50.0;  // 50 bps
    configSlip.riskPerTrade = 0.5;
    BacktestEngine engineSlip(configSlip);

    BuySellFixedStrategy s1;
    BuySellFixedStrategy s2;
    const auto resultZero = engineZero.run(s1, data);
    const auto resultSlip = engineSlip.run(s2, data);

    EXPECT_GT(resultZero.equityCurve.back(), resultSlip.equityCurve.back());
}

TEST(BacktestEngineTest, EmptyData_Throws) {
    PriceHistory data("EMPTY");
    data.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/empty.csv");

    BacktestEngine engine(BacktestConfig{});
    AlwaysHoldStrategy strategy;

    EXPECT_THROW(engine.run(strategy, data), std::invalid_argument);
}

TEST(BacktestEngineTest, MetricsArePopulated) {
    PriceHistory data("AAPL");
    data.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");

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
    EXPECT_GE(result.maxDrawdown, 0.0);
    EXPECT_LE(result.maxDrawdown, 1.0);
}
