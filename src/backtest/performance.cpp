#include "backtest/performance.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

#include "core/time_utils.h"

namespace barcola {

double calculateSharpeRatio(const std::vector<double>& equityCurve,
                            double riskFreeRate) {
    if (equityCurve.size() < 2) {
        return 0.0;
    }

    // Compute daily returns
    std::vector<double> dailyReturns;
    dailyReturns.reserve(equityCurve.size() - 1);
    for (size_t i = 1; i < equityCurve.size(); ++i) {
        if (equityCurve[i - 1] <= 0.0) {
            continue;
        }
        dailyReturns.push_back(equityCurve[i] / equityCurve[i - 1] - 1.0);
    }

    if (dailyReturns.empty()) {
        return 0.0;
    }

    const double dailyRiskFree = riskFreeRate / TRADING_DAYS_PER_YEAR;

    // Mean excess return
    double sum = 0.0;
    for (double r : dailyReturns) {
        sum += (r - dailyRiskFree);
    }
    const double meanExcess = sum / static_cast<double>(dailyReturns.size());

    // Std dev of daily returns
    double sumSqDiff = 0.0;
    const double meanReturn = std::accumulate(dailyReturns.begin(),
                                              dailyReturns.end(), 0.0) /
                              static_cast<double>(dailyReturns.size());
    for (double r : dailyReturns) {
        const double diff = r - meanReturn;
        sumSqDiff += diff * diff;
    }
    const double stdDev = std::sqrt(sumSqDiff /
                                    static_cast<double>(dailyReturns.size()));

    if (stdDev < 1e-12) {
        return 0.0;
    }

    return (meanExcess / stdDev) * std::sqrt(TRADING_DAYS_PER_YEAR);
}

double calculateMaxDrawdown(const std::vector<double>& equityCurve) {
    if (equityCurve.size() < 2) {
        return 0.0;
    }

    double peak = equityCurve[0];
    double maxDD = 0.0;

    for (size_t i = 1; i < equityCurve.size(); ++i) {
        if (equityCurve[i] > peak) {
            peak = equityCurve[i];
        }
        if (peak > 0.0) {
            const double dd = (peak - equityCurve[i]) / peak;
            maxDD = std::max(maxDD, dd);
        }
    }

    return maxDD;
}

double calculateSortinoRatio(const std::vector<double>& equityCurve,
                             double riskFreeRate) {
    if (equityCurve.size() < 2) {
        return 0.0;
    }

    std::vector<double> dailyReturns;
    dailyReturns.reserve(equityCurve.size() - 1);
    for (size_t i = 1; i < equityCurve.size(); ++i) {
        if (equityCurve[i - 1] <= 0.0) {
            continue;
        }
        dailyReturns.push_back(equityCurve[i] / equityCurve[i - 1] - 1.0);
    }

    if (dailyReturns.empty()) {
        return 0.0;
    }

    const double dailyRiskFree = riskFreeRate / TRADING_DAYS_PER_YEAR;

    double sumExcess = 0.0;
    for (double r : dailyReturns) {
        sumExcess += (r - dailyRiskFree);
    }
    const double meanExcess = sumExcess / static_cast<double>(dailyReturns.size());

    // Downside deviation: only negative returns
    double sumDownsideSq = 0.0;
    for (double r : dailyReturns) {
        if (r < dailyRiskFree) {
            const double diff = r - dailyRiskFree;
            sumDownsideSq += diff * diff;
        }
    }
    const double downsideDev = std::sqrt(sumDownsideSq /
                                         static_cast<double>(dailyReturns.size()));

    if (downsideDev < 1e-12) {
        return 0.0;
    }

    return (meanExcess / downsideDev) * std::sqrt(TRADING_DAYS_PER_YEAR);
}

double calculateCalmarRatio(const std::vector<double>& equityCurve) {
    if (equityCurve.size() < 2) {
        return 0.0;
    }

    const double totalReturn = equityCurve.back() / equityCurve.front() - 1.0;
    const double years = static_cast<double>(equityCurve.size() - 1) /
                         TRADING_DAYS_PER_YEAR;

    if (years < 1e-12) {
        return 0.0;
    }

    const double annualizedReturn = std::pow(1.0 + totalReturn, 1.0 / years) - 1.0;
    const double maxDD = calculateMaxDrawdown(equityCurve);

    if (maxDD < 1e-12) {
        return 0.0;
    }

    return annualizedReturn / maxDD;
}

TradeStatistics analyzeTradeHistory(const std::vector<Trade>& trades) {
    TradeStatistics stats;
    stats.totalTrades = trades.size();

    if (trades.empty()) {
        return stats;
    }

    double totalWin = 0.0;
    double totalLoss = 0.0;
    double totalHoldDays = 0.0;

    for (const auto& trade : trades) {
        if (trade.pnl > 0.0) {
            stats.winningTrades++;
            totalWin += trade.pnl;
        } else if (trade.pnl < 0.0) {
            stats.losingTrades++;
            totalLoss += std::abs(trade.pnl);
        }

        // Estimate hold days from dates
        if (!trade.entryDate.empty() && !trade.exitDate.empty()) {
            const time_t entry = convertDateToEpoch(trade.entryDate);
            const time_t exit = convertDateToEpoch(trade.exitDate);
            totalHoldDays += static_cast<double>(exit - entry) / 86400.0;
        }
    }

    stats.winRate = static_cast<double>(stats.winningTrades) /
                    static_cast<double>(stats.totalTrades);

    if (stats.winningTrades > 0) {
        stats.avgWin = totalWin / static_cast<double>(stats.winningTrades);
    }
    if (stats.losingTrades > 0) {
        stats.avgLoss = totalLoss / static_cast<double>(stats.losingTrades);
    }

    stats.profitFactor = (totalLoss > 1e-12) ? (totalWin / totalLoss) : 0.0;
    stats.avgHoldDays = totalHoldDays / static_cast<double>(stats.totalTrades);

    return stats;
}

}  // namespace barcola
