#include "backtest/backtest_engine.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace barcola {

namespace {

constexpr double BPS_PER_UNIT = 10000.0;

// An open long position. shares == 0 means flat.
struct Holding {
    double shares = 0.0;
    double entryPrice = 0.0;
    double entryCommission = 0.0;
    std::string entryDate;

    [[nodiscard]] bool isLong() const { return shares > 0.0; }
};

}  // namespace

BacktestEngine::BacktestEngine(BacktestConfig config) : config_(std::move(config)) {
    if (!(config_.initialCapital > 0.0)) {
        throw std::invalid_argument("initialCapital must be positive");
    }
    if (!(config_.commissionRate >= 0.0)) {
        throw std::invalid_argument("commissionRate must be zero or positive");
    }
    if (!(config_.slippageBps >= 0.0)) {
        throw std::invalid_argument("slippageBps must be zero or positive");
    }
    if (!(config_.positionFraction > 0.0 && config_.positionFraction <= 1.0)) {
        throw std::invalid_argument("positionFraction must be in (0, 1]");
    }
    if (!(config_.riskFreeRate > -1.0) || !std::isfinite(config_.riskFreeRate)) {
        throw std::invalid_argument("riskFreeRate must be a finite rate above -100%");
    }
}

BacktestResult BacktestEngine::run(Strategy& strategy, const PriceHistory& data) const {
    const auto& points = data.getDataPoints();
    if (points.empty()) {
        throw std::invalid_argument("Cannot backtest on empty price data");
    }
    const size_t firstBar = config_.warmupBars;
    if (firstBar >= points.size()) {
        throw std::invalid_argument("warmupBars (" + std::to_string(firstBar) +
                                    ") leaves no bars to trade out of " +
                                    std::to_string(points.size()));
    }

    strategy.reset();

    BacktestResult result;
    result.equityCurve.reserve(points.size() - firstBar);
    size_t barsInMarket = 0;

    const double dailyCashRate = config_.riskFreeRate / TRADING_DAYS_PER_YEAR;
    double cash = config_.initialCapital;
    Holding holding;
    Signal pending = Signal::HOLD;

    const auto openLong = [&](const PricePoint& bar, double referencePrice) {
        const double execPrice = applySlippage(referencePrice, true);
        const double shares = calculateShares(cash, execPrice);
        if (shares <= 0.0) {
            return;  // Can't afford a single share; stay flat.
        }
        const double cost = shares * execPrice;
        const double commission = applyCommission(cost);
        cash -= cost + commission;
        holding = {shares, execPrice, commission, bar.getDateString()};
    };

    const auto closeLong = [&](const PricePoint& bar, double referencePrice) {
        const double execPrice = applySlippage(referencePrice, false);
        const double proceeds = holding.shares * execPrice;
        const double commission = applyCommission(proceeds);
        cash += proceeds - commission;

        Trade trade;
        trade.symbol = data.getAssetSymbol();
        trade.entryPrice = holding.entryPrice;
        trade.exitPrice = execPrice;
        trade.entryDate = holding.entryDate;
        trade.exitDate = bar.getDateString();
        trade.shares = holding.shares;
        trade.pnl = (execPrice - holding.entryPrice) * holding.shares - holding.entryCommission -
                    commission;
        trade.returnPct = (execPrice - holding.entryPrice) / holding.entryPrice;
        result.trades.push_back(trade);

        holding = {};
    };

    const auto execute = [&](Signal signal, const PricePoint& bar, double referencePrice) {
        if (signal == Signal::BUY && !holding.isLong()) {
            openLong(bar, referencePrice);
        } else if (signal == Signal::SELL && holding.isLong()) {
            closeLong(bar, referencePrice);
        }
    };

    for (size_t i = firstBar; i < points.size(); ++i) {
        const PricePoint& bar = points[i];

        if (i > firstBar) {
            cash *= 1.0 + dailyCashRate;
        }

        if (config_.fillTiming == FillTiming::NEXT_OPEN) {
            execute(pending, bar, bar.getOpening());
            pending = Signal::HOLD;
        }

        const Position position = holding.isLong() ? Position::LONG : Position::FLAT;
        const Signal signal = strategy.evaluate(BarView(points, i + 1), position);

        if (config_.fillTiming == FillTiming::SAME_CLOSE) {
            execute(signal, bar, bar.getClosing());
        } else {
            pending = signal;
        }

        result.equityCurve.push_back(cash + holding.shares * bar.getClosing());
        if (holding.isLong()) {
            ++barsInMarket;
        }
    }

    // Close any open position at the last close. An order still pending from the last bar has
    // no next open to fill at, so it is dropped.
    if (holding.isLong()) {
        closeLong(points.back(), points.back().getClosing());
        result.equityCurve.back() = cash;
    }

    const size_t periods = result.equityCurve.size() - 1;
    result.exposure = static_cast<double>(barsInMarket) /
                      static_cast<double>(result.equityCurve.size());
    result.totalReturn = result.equityCurve.back() / config_.initialCapital - 1.0;
    result.annualizedReturn = annualizeReturn(result.totalReturn, periods);
    result.sharpeRatio = calculateSharpeRatio(result.equityCurve, config_.riskFreeRate);
    result.sortinoRatio = calculateSortinoRatio(result.equityCurve, config_.riskFreeRate);
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
    const double slippageFraction = config_.slippageBps / BPS_PER_UNIT;
    return isBuy ? price * (1.0 + slippageFraction) : price * (1.0 - slippageFraction);
}

double BacktestEngine::applyCommission(double tradeValue) const {
    return tradeValue * config_.commissionRate;
}

// Whole shares only. The entry commission is included so a full-cash entry never overdraws.
double BacktestEngine::calculateShares(double cash, double price) const {
    if (price <= 0.0 || cash <= 0.0) {
        return 0.0;
    }
    const double budget = cash * config_.positionFraction;
    return std::floor(budget / (price * (1.0 + config_.commissionRate)));
}

}  // namespace barcola
