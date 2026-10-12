#pragma once

#include <vector>

#include "backtest/performance.h"
#include "backtest/strategy.h"
#include "core/price_history.h"

namespace barcola {

// When an order decided on a bar's close is filled.
enum class FillTiming {
    NEXT_OPEN,   // Realistic: a signal computed from bar i's close fills at bar i+1's open.
    SAME_CLOSE,  // Optimistic: fill at the same close the signal was computed from.
};

struct BacktestConfig {
    double initialCapital = 100000.0;
    double commissionRate = 0.001;   // 0.1% of trade value, charged on entry and on exit
    double slippageBps = 5.0;        // 5 basis points against us on every fill
    double positionFraction = 1.0;   // fraction of available cash committed to each entry
    // Annual rate earned on idle cash. Also the hurdle for Sharpe and Sortino, so a strategy
    // that sits in cash scores zero instead of being penalised for not trading.
    double riskFreeRate = DEFAULT_RISK_FREE_RATE;
    FillTiming fillTiming = FillTiming::NEXT_OPEN;
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
    // Throws std::invalid_argument if any config value is out of range.
    explicit BacktestEngine(BacktestConfig config);

    // Long-only, one position at a time. Calls strategy.reset() before the first bar.
    [[nodiscard]] BacktestResult run(Strategy& strategy, const PriceHistory& data) const;

private:
    BacktestConfig config_;

    [[nodiscard]] double applySlippage(double price, bool isBuy) const;
    [[nodiscard]] double applyCommission(double tradeValue) const;
    [[nodiscard]] double calculateShares(double cash, double price) const;
};

}  // namespace barcola
