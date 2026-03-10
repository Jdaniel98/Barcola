#include <gtest/gtest.h>
#include <cmath>
#include "backtest/performance.h"

using namespace barcola;

// --- Sharpe Ratio ---

TEST(PerformanceTest, SharpeRatio_FlatEquity_ReturnsZero) {
    // No returns => zero Sharpe
    std::vector<double> equity = {100000, 100000, 100000, 100000, 100000};
    EXPECT_DOUBLE_EQ(calculateSharpeRatio(equity), 0.0);
}

TEST(PerformanceTest, SharpeRatio_GrowthWithVariance_Positive) {
    // Alternating growth rates to create non-zero variance
    std::vector<double> equity;
    double val = 100000.0;
    for (int i = 0; i < 252; ++i) {
        equity.push_back(val);
        val *= (i % 2 == 0) ? 1.002 : 1.0005;
    }
    const double sharpe = calculateSharpeRatio(equity);
    EXPECT_GT(sharpe, 0.0);
}

TEST(PerformanceTest, SharpeRatio_TooFewPoints_ReturnsZero) {
    std::vector<double> equity = {100000};
    EXPECT_DOUBLE_EQ(calculateSharpeRatio(equity), 0.0);
}

// --- Max Drawdown ---

TEST(PerformanceTest, MaxDrawdown_NoDecline_Zero) {
    std::vector<double> equity = {100, 110, 120, 130, 140};
    EXPECT_DOUBLE_EQ(calculateMaxDrawdown(equity), 0.0);
}

TEST(PerformanceTest, MaxDrawdown_KnownValue) {
    // Peak at 200, drops to 150 => 25% drawdown
    std::vector<double> equity = {100, 150, 200, 180, 150, 190};
    EXPECT_NEAR(calculateMaxDrawdown(equity), 0.25, 1e-10);
}

TEST(PerformanceTest, MaxDrawdown_FullDecline) {
    // Drops from 100 to 50 => 50% drawdown
    std::vector<double> equity = {100, 80, 60, 50, 70};
    EXPECT_NEAR(calculateMaxDrawdown(equity), 0.50, 1e-10);
}

// --- Sortino Ratio ---

TEST(PerformanceTest, SortinoRatio_MixedReturns_Positive) {
    // Growth with some down days to create downside deviation
    std::vector<double> equity;
    double val = 100000.0;
    for (int i = 0; i < 100; ++i) {
        equity.push_back(val);
        val *= (i % 5 == 0) ? 0.998 : 1.003;  // mostly up, occasional down
    }
    const double sortino = calculateSortinoRatio(equity);
    EXPECT_GT(sortino, 0.0);
}

TEST(PerformanceTest, SortinoRatio_TooFewPoints_ReturnsZero) {
    std::vector<double> equity = {100000};
    EXPECT_DOUBLE_EQ(calculateSortinoRatio(equity), 0.0);
}

// --- Calmar Ratio ---

TEST(PerformanceTest, CalmarRatio_NoDrawdown_ReturnsZero) {
    // No drawdown => division by zero guard returns 0
    std::vector<double> equity = {100, 110, 120, 130, 140};
    EXPECT_DOUBLE_EQ(calculateCalmarRatio(equity), 0.0);
}

TEST(PerformanceTest, CalmarRatio_WithDrawdown_Positive) {
    // Generate a year of data with some drawdown
    std::vector<double> equity;
    double val = 100000.0;
    for (int i = 0; i < 252; ++i) {
        equity.push_back(val);
        if (i < 50) val *= 1.002;       // growth
        else if (i < 100) val *= 0.998; // decline
        else val *= 1.001;              // recovery
    }
    const double calmar = calculateCalmarRatio(equity);
    EXPECT_GT(calmar, 0.0);
}

// --- Trade Statistics ---

TEST(PerformanceTest, TradeStats_Empty) {
    std::vector<Trade> trades;
    const auto stats = analyzeTradeHistory(trades);
    EXPECT_EQ(stats.totalTrades, 0u);
    EXPECT_DOUBLE_EQ(stats.winRate, 0.0);
}

TEST(PerformanceTest, TradeStats_MixedTrades) {
    std::vector<Trade> trades;

    Trade win;
    win.pnl = 500.0;
    win.entryDate = "2023-01-03";
    win.exitDate = "2023-01-10";
    trades.push_back(win);

    Trade loss;
    loss.pnl = -200.0;
    loss.entryDate = "2023-01-11";
    loss.exitDate = "2023-01-15";
    trades.push_back(loss);

    Trade win2;
    win2.pnl = 300.0;
    win2.entryDate = "2023-01-16";
    win2.exitDate = "2023-01-20";
    trades.push_back(win2);

    const auto stats = analyzeTradeHistory(trades);
    EXPECT_EQ(stats.totalTrades, 3u);
    EXPECT_EQ(stats.winningTrades, 2u);
    EXPECT_EQ(stats.losingTrades, 1u);
    EXPECT_NEAR(stats.winRate, 2.0 / 3.0, 1e-10);
    EXPECT_NEAR(stats.avgWin, 400.0, 1e-10);    // (500+300)/2
    EXPECT_NEAR(stats.avgLoss, 200.0, 1e-10);
    EXPECT_NEAR(stats.profitFactor, 800.0 / 200.0, 1e-10);  // 4.0
}
