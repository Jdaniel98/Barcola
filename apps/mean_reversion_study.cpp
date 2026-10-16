// Phase 3 study: z-score mean reversion against buy-and-hold on daily prices.
//
// Parameters are chosen on the in-sample window only (2014-2019), by the highest mean
// Sharpe ratio across all symbols, then evaluated unchanged on the out-of-sample window
// (2020-2024). The full parameter grid is also run out-of-sample so the write-up can show
// how sensitive the result is to the choice.
//
// Usage: mean_reversion_study [data_dir] [results_dir]
//   data_dir     folder of <SYMBOL>.csv files (default: data/market)
//   results_dir  folder for the CSV outputs (default: results)

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "backtest/backtest_engine.h"
#include "buy_and_hold_strategy.h"
#include "core/price_history.h"
#include "core/time_utils.h"
#include "mean_reversion_strategy.h"

namespace fs = std::filesystem;
using namespace barcola;

namespace {

struct Window {
    std::string name;
    std::string start;
    std::string end;
};

const Window IN_SAMPLE{"in_sample", "2014-01-01", "2019-12-31"};
const Window OUT_OF_SAMPLE{"out_of_sample", "2020-01-01", "2024-12-31"};

const std::vector<size_t> LOOKBACKS = {10, 20, 30, 50};
const std::vector<double> ENTRY_Z = {1.0, 1.5, 2.0, 2.5};
const std::vector<double> EXIT_Z = {0.0, 0.5};

// Every run gets the same warm-up so all parameter sets trade over identical windows.
// MeanReversionStrategy needs lookback + 1 bars before its first signal.
constexpr size_t WARMUP_BARS = 51;

constexpr double INITIAL_CAPITAL = 100000.0;
constexpr double COMMISSION_RATE = 0.001;  // 0.1% per side
constexpr double SLIPPAGE_BPS = 5.0;       // per side
constexpr double RISK_FREE_RATE = 0.0;     // no interest on cash and no Sharpe hurdle

struct Params {
    size_t lookback = 0;
    double entryZ = 0.0;
    double exitZ = 0.0;
};

struct Row {
    std::string window;
    std::string strategy;
    std::string symbol;
    Params params;
    BacktestResult result;
};

// The window's bars plus up to WARMUP_BARS bars of history before it.
struct WindowData {
    PriceHistory bars;
    size_t warmupBars = 0;
};

WindowData sliceWindow(const PriceHistory& history, const Window& window) {
    const time_t start = convertDateToEpoch(window.start);
    const time_t end = convertDateToEpoch(window.end);
    const auto& points = history.getDataPoints();
    const auto firstInWindow = std::lower_bound(
        points.begin(), points.end(), start,
        [](const PricePoint& point, time_t value) { return point.getDate() < value; });
    const size_t windowIndex = static_cast<size_t>(firstInWindow - points.begin());
    if (windowIndex >= points.size()) {
        throw std::runtime_error(history.getAssetSymbol() + " has no data in " + window.name);
    }
    const size_t warmup = std::min(windowIndex, WARMUP_BARS);
    if (warmup < WARMUP_BARS) {
        std::cerr << "warning: " << history.getAssetSymbol() << " has only " << warmup
                  << " warm-up bars before " << window.start << "\n";
    }
    return {history.between(points[windowIndex - warmup].getDate(), end), warmup};
}

BacktestResult runStrategy(Strategy& strategy, const WindowData& data) {
    BacktestConfig config;
    config.initialCapital = INITIAL_CAPITAL;
    config.commissionRate = COMMISSION_RATE;
    config.slippageBps = SLIPPAGE_BPS;
    config.positionFraction = 1.0;
    config.riskFreeRate = RISK_FREE_RATE;
    config.fillTiming = FillTiming::NEXT_OPEN;
    config.warmupBars = data.warmupBars;
    return BacktestEngine(config).run(strategy, data.bars);
}

std::string paramLabel(const Params& p) {
    std::ostringstream out;
    out << "lookback=" << p.lookback << " entryZ=" << p.entryZ << " exitZ=" << p.exitZ;
    return out.str();
}

std::vector<PriceHistory> loadUniverse(const fs::path& dataDir) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".csv") {
            files.push_back(entry.path());
        }
    }
    if (files.empty()) {
        throw std::runtime_error("No .csv files in " + dataDir.string() +
                                 ". Run scripts/fetch_market_data.sh first.");
    }
    std::sort(files.begin(), files.end());

    std::vector<PriceHistory> universe;
    for (const auto& file : files) {
        PriceHistory history(file.stem().string());
        history.loadFromCsv(file.string());
        universe.push_back(history);
    }
    return universe;
}

void writeMetricsCsv(const fs::path& path, const std::vector<Row>& rows) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Cannot write " + path.string());
    }
    out << "window,strategy,symbol,lookback,entry_z,exit_z,total_return,cagr,sharpe,sortino,"
           "max_drawdown,calmar,trades,win_rate,profit_factor,avg_hold_days,exposure\n";
    out << std::setprecision(10);
    for (const auto& row : rows) {
        const auto& r = row.result;
        out << row.window << "," << row.strategy << "," << row.symbol << ","
            << row.params.lookback << "," << row.params.entryZ << "," << row.params.exitZ << ","
            << r.totalReturn << "," << r.annualizedReturn << "," << r.sharpeRatio << ","
            << r.sortinoRatio << "," << r.maxDrawdown << "," << r.calmarRatio << ","
            << r.totalTrades << "," << r.winRate << "," << r.profitFactor << ","
            << r.avgHoldDays << "," << r.exposure << "\n";
    }
}

std::string percent(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value * 100.0 << "%";
    return out.str();
}

std::string fixed2(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}

// Markdown table comparing mean reversion and buy-and-hold for one window.
void printComparison(const std::string& title, const std::vector<Row>& meanReversion,
                     const std::vector<Row>& buyAndHold) {
    std::cout << "\n### " << title << "\n\n"
              << "| Symbol | MR CAGR | MR Sharpe | MR max DD | Trades | Win rate | In market "
                 "| B&H CAGR | B&H Sharpe | B&H max DD |\n"
              << "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";

    const auto mean = [](const std::vector<Row>& rows, auto field) {
        double sum = 0.0;
        for (const auto& row : rows) {
            sum += field(row.result);
        }
        return sum / static_cast<double>(rows.size());
    };

    for (size_t i = 0; i < meanReversion.size(); ++i) {
        const auto& mr = meanReversion[i].result;
        const auto& bh = buyAndHold[i].result;
        std::cout << "| " << meanReversion[i].symbol << " | " << percent(mr.annualizedReturn)
                  << " | " << fixed2(mr.sharpeRatio) << " | " << percent(mr.maxDrawdown) << " | "
                  << mr.totalTrades << " | " << percent(mr.winRate) << " | "
                  << percent(mr.exposure) << " | " << percent(bh.annualizedReturn) << " | "
                  << fixed2(bh.sharpeRatio) << " | " << percent(bh.maxDrawdown) << " |\n";
    }

    const auto cagr = [](const BacktestResult& r) { return r.annualizedReturn; };
    const auto sharpe = [](const BacktestResult& r) { return r.sharpeRatio; };
    const auto drawdown = [](const BacktestResult& r) { return r.maxDrawdown; };
    const auto trades = [](const BacktestResult& r) { return static_cast<double>(r.totalTrades); };
    const auto winRate = [](const BacktestResult& r) { return r.winRate; };
    const auto exposure = [](const BacktestResult& r) { return r.exposure; };
    std::cout << "| **Average** | " << percent(mean(meanReversion, cagr)) << " | "
              << fixed2(mean(meanReversion, sharpe)) << " | "
              << percent(mean(meanReversion, drawdown)) << " | "
              << fixed2(mean(meanReversion, trades)) << " | "
              << percent(mean(meanReversion, winRate)) << " | "
              << percent(mean(meanReversion, exposure)) << " | "
              << percent(mean(buyAndHold, cagr)) << " | " << fixed2(mean(buyAndHold, sharpe))
              << " | " << percent(mean(buyAndHold, drawdown)) << " |\n";
}

// Trade outcomes grouped by how long the position was held (calendar days), pooled
// across symbols.
void printHoldingPeriods(const std::string& title, const std::vector<Row>& rows) {
    struct Bucket {
        const char* label;
        double minDays;
        double maxDays;
    };
    constexpr double SECONDS_PER_DAY = 86400.0;
    const Bucket buckets[] = {{"0-14 days", 0, 14},
                              {"15-30 days", 15, 30},
                              {"31-60 days", 31, 60},
                              {"61+ days", 61, 1e9}};

    std::cout << "\n### Trades by holding period, " << title << "\n\n"
              << "| Held | Trades | Win rate | Average return |\n|---|---:|---:|---:|\n";
    for (const auto& bucket : buckets) {
        size_t count = 0;
        size_t wins = 0;
        double returnSum = 0.0;
        for (const auto& row : rows) {
            for (const auto& trade : row.result.trades) {
                const double days = static_cast<double>(convertDateToEpoch(trade.exitDate) -
                                                        convertDateToEpoch(trade.entryDate)) /
                                    SECONDS_PER_DAY;
                if (days < bucket.minDays || days > bucket.maxDays) {
                    continue;
                }
                ++count;
                wins += trade.returnPct > 0.0 ? 1 : 0;
                returnSum += trade.returnPct;
            }
        }
        if (count == 0) {
            continue;
        }
        const double n = static_cast<double>(count);
        std::cout << "| " << bucket.label << " | " << count << " | "
                  << percent(static_cast<double>(wins) / n) << " | " << percent(returnSum / n)
                  << " |\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const fs::path dataDir = argc > 1 ? argv[1] : "data/market";
        const fs::path resultsDir = argc > 2 ? argv[2] : "results";
        fs::create_directories(resultsDir);

        const std::vector<PriceHistory> universe = loadUniverse(dataDir);
        std::vector<WindowData> inSample;
        std::vector<WindowData> outOfSample;
        for (const auto& history : universe) {
            inSample.push_back(sliceWindow(history, IN_SAMPLE));
            outOfSample.push_back(sliceWindow(history, OUT_OF_SAMPLE));
        }

        // 1. Run the whole grid on both windows. Only the in-sample results pick the winner.
        std::vector<Params> grid;
        for (size_t lookback : LOOKBACKS) {
            for (double entryZ : ENTRY_Z) {
                for (double exitZ : EXIT_Z) {
                    grid.push_back({lookback, entryZ, exitZ});
                }
            }
        }

        std::vector<Row> inSampleGrid;
        std::vector<Row> outOfSampleGrid;
        std::vector<double> meanInSampleSharpe;
        for (const Params& params : grid) {
            MeanReversionStrategy strategy(params.lookback, params.entryZ, params.exitZ);
            double sharpeSum = 0.0;
            for (size_t s = 0; s < universe.size(); ++s) {
                const std::string& symbol = universe[s].getAssetSymbol();
                const BacktestResult in = runStrategy(strategy, inSample[s]);
                sharpeSum += in.sharpeRatio;
                inSampleGrid.push_back({IN_SAMPLE.name, "MeanReversion", symbol, params, in});
                outOfSampleGrid.push_back({OUT_OF_SAMPLE.name, "MeanReversion", symbol, params,
                                           runStrategy(strategy, outOfSample[s])});
            }
            meanInSampleSharpe.push_back(sharpeSum / static_cast<double>(universe.size()));
        }

        const size_t best = static_cast<size_t>(
            std::max_element(meanInSampleSharpe.begin(), meanInSampleSharpe.end()) -
            meanInSampleSharpe.begin());
        const Params chosen = grid[best];

        // Where the in-sample choice ranks among all parameter sets out-of-sample
        std::vector<double> meanOutOfSampleSharpe(grid.size(), 0.0);
        for (size_t g = 0; g < grid.size(); ++g) {
            for (size_t s = 0; s < universe.size(); ++s) {
                meanOutOfSampleSharpe[g] +=
                    outOfSampleGrid[g * universe.size() + s].result.sharpeRatio;
            }
            meanOutOfSampleSharpe[g] /= static_cast<double>(universe.size());
        }
        const size_t outOfSampleRank =
            1 + static_cast<size_t>(std::count_if(
                    meanOutOfSampleSharpe.begin(), meanOutOfSampleSharpe.end(),
                    [&](double sharpe) { return sharpe > meanOutOfSampleSharpe[best]; }));

        // 2. Chosen parameters and the buy-and-hold benchmark on both windows
        std::vector<Row> summary;
        std::vector<Row> mrIn, mrOut, bhIn, bhOut;
        BuyAndHoldStrategy buyAndHold;
        for (size_t s = 0; s < universe.size(); ++s) {
            const std::string& symbol = universe[s].getAssetSymbol();
            mrIn.push_back(inSampleGrid[best * universe.size() + s]);
            mrOut.push_back(outOfSampleGrid[best * universe.size() + s]);
            bhIn.push_back(
                {IN_SAMPLE.name, "BuyAndHold", symbol, {}, runStrategy(buyAndHold, inSample[s])});
            bhOut.push_back({OUT_OF_SAMPLE.name, "BuyAndHold", symbol, {},
                             runStrategy(buyAndHold, outOfSample[s])});
        }
        for (const auto* rows : {&mrIn, &bhIn, &mrOut, &bhOut}) {
            summary.insert(summary.end(), rows->begin(), rows->end());
        }

        // 3. Outputs
        writeMetricsCsv(resultsDir / "in_sample_grid.csv", inSampleGrid);
        writeMetricsCsv(resultsDir / "out_of_sample_grid.csv", outOfSampleGrid);
        writeMetricsCsv(resultsDir / "summary.csv", summary);

        // Trades of the chosen parameters in both windows
        std::ofstream trades(resultsDir / "trades.csv");
        trades << "window,symbol,entry_date,exit_date,entry_price,exit_price,shares,pnl,return\n"
               << std::setprecision(10);
        for (const auto* rows : {&mrIn, &mrOut}) {
            for (const auto& row : *rows) {
                for (const auto& t : row.result.trades) {
                    trades << row.window << "," << t.symbol << "," << t.entryDate << ","
                           << t.exitDate << "," << t.entryPrice << "," << t.exitPrice << ","
                           << t.shares << "," << t.pnl << "," << t.returnPct << "\n";
                }
            }
        }

        std::ofstream equity(resultsDir / "equity_out_of_sample.csv");
        equity << "date,symbol,strategy,equity\n" << std::setprecision(10);
        for (const auto* rows : {&mrOut, &bhOut}) {
            for (size_t s = 0; s < rows->size(); ++s) {
                const auto& points = outOfSample[s].bars.getDataPoints();
                const auto& curve = (*rows)[s].result.equityCurve;
                for (size_t k = 0; k < curve.size(); ++k) {
                    equity << points[outOfSample[s].warmupBars + k].getDateString() << ","
                           << (*rows)[s].symbol << "," << (*rows)[s].strategy << ","
                           << curve[k] << "\n";
                }
            }
        }

        std::cout << "## Mean reversion study\n\n"
                  << "Symbols: " << universe.size() << ". Costs: " << COMMISSION_RATE * 100.0
                  << "% commission + " << SLIPPAGE_BPS
                  << " bps slippage per side. Fills at next open. No interest on cash.\n\n"
                  << "Chosen in-sample (" << IN_SAMPLE.start << " to " << IN_SAMPLE.end
                  << "): " << paramLabel(chosen) << ", mean Sharpe "
                  << fixed2(meanInSampleSharpe[best]) << " (best of " << grid.size() << ").\n"
                  << "Out-of-sample it ranks " << outOfSampleRank << " of " << grid.size()
                  << " parameter sets by mean Sharpe ("
                  << fixed2(meanOutOfSampleSharpe[best]) << "; best "
                  << fixed2(*std::max_element(meanOutOfSampleSharpe.begin(),
                                              meanOutOfSampleSharpe.end()))
                  << ", median ";
        std::vector<double> sorted = meanOutOfSampleSharpe;
        std::sort(sorted.begin(), sorted.end());
        std::cout << fixed2((sorted[(sorted.size() - 1) / 2] + sorted[sorted.size() / 2]) / 2.0)
                  << ").\n";

        double buyAndHoldSharpe = 0.0;
        for (const auto& row : bhOut) {
            buyAndHoldSharpe += row.result.sharpeRatio;
        }
        buyAndHoldSharpe /= static_cast<double>(bhOut.size());
        const auto countAbove = [&](double threshold) {
            return std::count_if(meanOutOfSampleSharpe.begin(), meanOutOfSampleSharpe.end(),
                                 [&](double sharpe) { return sharpe > threshold; });
        };
        std::cout << "Out-of-sample, " << countAbove(0.0) << " of " << grid.size()
                  << " parameter sets have a positive mean Sharpe and "
                  << countAbove(buyAndHoldSharpe) << " beat buy-and-hold's mean Sharpe of "
                  << fixed2(buyAndHoldSharpe) << ".\n";

        printComparison("In-sample " + IN_SAMPLE.start + " to " + IN_SAMPLE.end, mrIn, bhIn);
        printComparison("Out-of-sample " + OUT_OF_SAMPLE.start + " to " + OUT_OF_SAMPLE.end,
                        mrOut, bhOut);
        printHoldingPeriods("in-sample", mrIn);
        printHoldingPeriods("out-of-sample", mrOut);
        std::cout << "\nCSV results written to " << resultsDir.string() << "/\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
