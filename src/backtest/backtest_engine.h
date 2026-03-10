#pragma once

#include <vector>
#include "backtest/strategy.h"
#include "backtest/performance.h"
#include "core/price_history.h"

namespace barcola {

struct BacktestConfig {
    double initialCapital = 100000.0;
    double commissionRate = 0.001;    // 0.1% per trade
    double slippageBps = 5.0;        // 5 basis points
    double riskPerTrade = 0.02;      // 2% risk per trade
};

struct BacktestResult {
    std::vector<Trade> trades;
    std::vector<double> equityCurve;
    double totalReturn = 0.0;
    double annualizedReturn = 0.0;
    double sharpeRatio = 0.0;
    double sortinoRatio = 0.0;
    double maxDrawdown = 0.0;
    double calmarRatio = 0.0;
    double winRate = 0.0;
    double profitFactor = 0.0;
    size_t totalTrades = 0;
    double avgHoldDays = 0.0;
};

class BacktestEngine {
public:
    explicit BacktestEngine(BacktestConfig config);

    [[nodiscard]] BacktestResult run(Strategy& strategy,
                                     const PriceHistory& data) const;

private:
    BacktestConfig config_;

    [[nodiscard]] double applySlippage(double price, bool isBuy) const;
    [[nodiscard]] double applyCommission(double tradeValue) const;
    [[nodiscard]] double calculateShares(double capital, double price) const;
};

}  // namespace barcola
