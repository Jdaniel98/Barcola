#pragma once

#include <cstddef>
#include <vector>
#include "backtest/strategy.h"

namespace barcola {

constexpr int TRADING_DAYS_PER_YEAR = 252;
constexpr double DEFAULT_RISK_FREE_RATE = 0.05;

struct TradeStatistics {
    size_t totalTrades = 0;
    size_t winningTrades = 0;
    size_t losingTrades = 0;
    double winRate = 0.0;
    double avgWin = 0.0;
    double avgLoss = 0.0;
    double profitFactor = 0.0;
    double avgHoldDays = 0.0;
};

[[nodiscard]] double calculateSharpeRatio(
    const std::vector<double>& equityCurve,
    double riskFreeRate = DEFAULT_RISK_FREE_RATE);

[[nodiscard]] double calculateMaxDrawdown(
    const std::vector<double>& equityCurve);

[[nodiscard]] double calculateSortinoRatio(
    const std::vector<double>& equityCurve,
    double riskFreeRate = DEFAULT_RISK_FREE_RATE);

[[nodiscard]] double calculateCalmarRatio(
    const std::vector<double>& equityCurve);

[[nodiscard]] TradeStatistics analyzeTradeHistory(
    const std::vector<Trade>& trades);

}  // namespace barcola
