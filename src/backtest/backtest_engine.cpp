#include "backtest/backtest_engine.h"

#include <cmath>
#include <stdexcept>

namespace barcola {

BacktestEngine::BacktestEngine(BacktestConfig config)
    : config_(std::move(config)) {}

BacktestResult BacktestEngine::run(Strategy& strategy,
                                   const PriceHistory& data) const {
    const auto& points = data.getDataPoints();
    if (points.empty()) {
        throw std::invalid_argument("Cannot backtest on empty price data");
    }

    BacktestResult result;
    double cash = config_.initialCapital;
    double shares = 0.0;
    double entryPrice = 0.0;
    std::string entryDate;
    bool inPosition = false;

    result.equityCurve.reserve(points.size());

    for (size_t i = 0; i < points.size(); ++i) {
        const double price = points[i].getClosing();
        const Signal signal = strategy.evaluate(points, i);

        if (signal == Signal::BUY && !inPosition) {
            // Enter long position
            const double execPrice = applySlippage(price, true);
            shares = calculateShares(cash, execPrice);
            if (shares > 0.0) {
                const double cost = shares * execPrice;
                const double commission = applyCommission(cost);
                cash -= (cost + commission);
                entryPrice = execPrice;
                entryDate = points[i].getDateString();
                inPosition = true;
            }
        } else if (signal == Signal::SELL && inPosition) {
            // Exit long position
            const double execPrice = applySlippage(price, false);
            const double proceeds = shares * execPrice;
            const double commission = applyCommission(proceeds);
            cash += (proceeds - commission);

            Trade trade;
            trade.symbol = data.getAssetSymbol();
            trade.entryPrice = entryPrice;
            trade.exitPrice = execPrice;
            trade.entryDate = entryDate;
            trade.exitDate = points[i].getDateString();
            trade.shares = shares;
            trade.pnl = (execPrice - entryPrice) * shares -
                         applyCommission(shares * entryPrice) -
                         applyCommission(proceeds);
            trade.returnPct = (execPrice - entryPrice) / entryPrice;
            result.trades.push_back(trade);

            shares = 0.0;
            inPosition = false;
        }

        // Record equity: cash + mark-to-market position value
        const double portfolioValue = cash + shares * price;
        result.equityCurve.push_back(portfolioValue);
    }

    // Close any open position at the last price
    if (inPosition && !points.empty()) {
        const double lastPrice = points.back().getClosing();
        const double execPrice = applySlippage(lastPrice, false);
        const double proceeds = shares * execPrice;
        const double commission = applyCommission(proceeds);
        cash += (proceeds - commission);

        Trade trade;
        trade.symbol = data.getAssetSymbol();
        trade.entryPrice = entryPrice;
        trade.exitPrice = execPrice;
        trade.entryDate = entryDate;
        trade.exitDate = points.back().getDateString();
        trade.shares = shares;
        trade.pnl = (execPrice - entryPrice) * shares -
                     applyCommission(shares * entryPrice) -
                     applyCommission(proceeds);
        trade.returnPct = (execPrice - entryPrice) / entryPrice;
        result.trades.push_back(trade);

        shares = 0.0;
        result.equityCurve.back() = cash;
    }

    // Compute summary metrics
    const double finalEquity = result.equityCurve.back();
    result.totalReturn = (finalEquity / config_.initialCapital) - 1.0;

    const double years = static_cast<double>(points.size()) /
                         TRADING_DAYS_PER_YEAR;
    if (years > 1e-12) {
        result.annualizedReturn =
            std::pow(1.0 + result.totalReturn, 1.0 / years) - 1.0;
    }

    result.sharpeRatio = calculateSharpeRatio(result.equityCurve);
    result.sortinoRatio = calculateSortinoRatio(result.equityCurve);
    result.maxDrawdown = calculateMaxDrawdown(result.equityCurve);
    result.calmarRatio = calculateCalmarRatio(result.equityCurve);

    const TradeStatistics stats = analyzeTradeHistory(result.trades);
    result.winRate = stats.winRate;
    result.profitFactor = stats.profitFactor;
    result.totalTrades = stats.totalTrades;
    result.avgHoldDays = stats.avgHoldDays;

    return result;
}

double BacktestEngine::applySlippage(double price, bool isBuy) const {
    const double slippageFraction = config_.slippageBps / 10000.0;
    return isBuy ? price * (1.0 + slippageFraction)
                 : price * (1.0 - slippageFraction);
}

double BacktestEngine::applyCommission(double tradeValue) const {
    return tradeValue * config_.commissionRate;
}

double BacktestEngine::calculateShares(double capital, double price) const {
    if (price <= 0.0) {
        return 0.0;
    }
    const double riskCapital = capital * config_.riskPerTrade;
    return std::floor(riskCapital / price);
}

}  // namespace barcola
