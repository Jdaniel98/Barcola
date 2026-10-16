// Strategy study: each strategy family against buy-and-hold and against each other on
// daily prices.
//
// For every family, one parameter set is chosen on the in-sample window only (2014-2019),
// by the highest mean Sharpe ratio across all symbols, then run unchanged on the
// out-of-sample window (2020-2024). Each family's full parameter grid is also run
// out-of-sample, so the write-up can show how much the choice matters.
//
// Usage: strategy_study [data_dir] [results_dir]
//   data_dir     folder of <SYMBOL>.csv files (default: data/market)
//   results_dir  folder for the outputs (default: results)

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "analysis/correlation.h"
#include "backtest/backtest_engine.h"
#include "buy_and_hold_strategy.h"
#include "core/price_history.h"
#include "core/time_utils.h"
#include "ma_crossover_strategy.h"
#include "mean_reversion_strategy.h"

namespace fs = std::filesystem;
using namespace barcola;

namespace {

struct Window {
    std::string name;
    std::string label;
    std::string start;
    std::string end;
};

const Window IN_SAMPLE{"in_sample", "In-sample", "2014-01-01", "2019-12-31"};
const Window OUT_OF_SAMPLE{"out_of_sample", "Out-of-sample", "2020-01-01", "2024-12-31"};

// One year of history before each window. Every run gets the same history, so all
// parameter sets trade over identical days; the longest lookback (200 days) fits inside it.
constexpr size_t WARMUP_BARS = 252;

constexpr double INITIAL_CAPITAL = 100000.0;
constexpr double COMMISSION_RATE = 0.001;  // 0.1% per side
constexpr double SLIPPAGE_BPS = 5.0;       // per side
constexpr double RISK_FREE_RATE = 0.0;     // no interest on cash and no Sharpe hurdle

using Params = std::vector<double>;

// A strategy with a parameter grid to choose from.
struct Family {
    std::string name;       // as written to the CSV files, e.g. "MeanReversion"
    std::string shortName;  // table column prefix, e.g. "MR"
    std::string fileStem;   // grid file prefix, e.g. "mean_reversion"
    std::vector<std::string> paramNames;
    std::vector<Params> grid;
    std::function<std::unique_ptr<Strategy>(const Params&)> make;
};

std::vector<Family> studyFamilies() {
    Family meanReversion{"MeanReversion", "MR", "mean_reversion",
                         {"lookback", "entry_z", "exit_z"}, {},
                         [](const Params& p) -> std::unique_ptr<Strategy> {
                             return std::make_unique<MeanReversionStrategy>(
                                 static_cast<size_t>(p[0]), p[1], p[2]);
                         }};
    for (double lookback : {10.0, 20.0, 30.0, 50.0}) {
        for (double entryZ : {1.0, 1.5, 2.0, 2.5}) {
            for (double exitZ : {0.0, 0.5}) {
                meanReversion.grid.push_back({lookback, entryZ, exitZ});
            }
        }
    }

    Family crossover{"MACrossover", "MA", "ma_crossover", {"short_window", "long_window"}, {},
                     [](const Params& p) -> std::unique_ptr<Strategy> {
                         return std::make_unique<MovingAverageCrossoverStrategy>(
                             static_cast<size_t>(p[0]), static_cast<size_t>(p[1]));
                     }};
    for (double shortWindow : {10.0, 20.0, 50.0}) {
        for (double longWindow : {50.0, 100.0, 200.0}) {
            if (shortWindow < longWindow) {
                crossover.grid.push_back({shortWindow, longWindow});
            }
        }
    }

    return {meanReversion, crossover};
}

struct Row {
    std::string window;
    std::string strategy;
    std::string symbol;
    std::string params;  // e.g. "lookback=20 entry_z=2 exit_z=0.5"; empty for buy-and-hold
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

std::string paramLabel(const Family& family, const Params& params) {
    std::ostringstream out;
    for (size_t i = 0; i < params.size(); ++i) {
        out << (i == 0 ? "" : " ") << family.paramNames[i] << "=" << params[i];
    }
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

std::ofstream openOutput(const fs::path& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Cannot write " + path.string());
    }
    out << std::setprecision(10);
    return out;
}

void writeMetrics(std::ostream& out, const BacktestResult& r) {
    out << r.totalReturn << "," << r.annualizedReturn << "," << r.sharpeRatio << ","
        << r.sortinoRatio << "," << r.maxDrawdown << "," << r.calmarRatio << ","
        << r.totalTrades << "," << r.winRate << "," << r.profitFactor << "," << r.avgHoldDays
        << "," << r.exposure << "\n";
}

const char* const METRIC_COLUMNS =
    "total_return,cagr,sharpe,sortino,max_drawdown,calmar,trades,win_rate,profit_factor,"
    "avg_hold_days,exposure";

// One row per parameter set and symbol, with each parameter in its own column.
void writeGridCsv(const fs::path& path, const Family& family, const std::vector<Params>& grid,
                  const std::vector<Row>& rows, size_t symbols) {
    std::ofstream out = openOutput(path);
    out << "window,strategy,symbol";
    for (const auto& name : family.paramNames) {
        out << "," << name;
    }
    out << "," << METRIC_COLUMNS << "\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        out << rows[i].window << "," << rows[i].strategy << "," << rows[i].symbol;
        for (double value : grid[i / symbols]) {
            out << "," << value;
        }
        out << ",";
        writeMetrics(out, rows[i].result);
    }
}

void writeSummaryCsv(const fs::path& path, const std::vector<Row>& rows) {
    std::ofstream out = openOutput(path);
    out << "window,strategy,symbol,params," << METRIC_COLUMNS << "\n";
    for (const auto& row : rows) {
        out << row.window << "," << row.strategy << "," << row.symbol << "," << row.params
            << ",";
        writeMetrics(out, row.result);
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

double meanOf(const std::vector<Row>& rows,
              const std::function<double(const BacktestResult&)>& field) {
    double sum = 0.0;
    for (const auto& row : rows) {
        sum += field(row.result);
    }
    return sum / static_cast<double>(rows.size());
}

double meanSharpe(const std::vector<Row>& rows) {
    return meanOf(rows, [](const BacktestResult& r) { return r.sharpeRatio; });
}

// Markdown table comparing one strategy with buy-and-hold, symbol by symbol, for one window.
void printComparison(const std::string& title, const std::string& prefix,
                     const std::vector<Row>& strategy, const std::vector<Row>& buyAndHold) {
    std::cout << "\n### " << title << "\n\n"
              << "| Symbol | " << prefix << " CAGR | " << prefix << " Sharpe | " << prefix
              << " max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD "
                 "|\n"
              << "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";

    for (size_t i = 0; i < strategy.size(); ++i) {
        const auto& s = strategy[i].result;
        const auto& bh = buyAndHold[i].result;
        std::cout << "| " << strategy[i].symbol << " | " << percent(s.annualizedReturn) << " | "
                  << fixed2(s.sharpeRatio) << " | " << percent(s.maxDrawdown) << " | "
                  << s.totalTrades << " | " << percent(s.winRate) << " | "
                  << percent(s.exposure) << " | " << percent(bh.annualizedReturn) << " | "
                  << fixed2(bh.sharpeRatio) << " | " << percent(bh.maxDrawdown) << " |\n";
    }

    const auto cagr = [](const BacktestResult& r) { return r.annualizedReturn; };
    const auto drawdown = [](const BacktestResult& r) { return r.maxDrawdown; };
    const auto trades = [](const BacktestResult& r) { return static_cast<double>(r.totalTrades); };
    const auto winRate = [](const BacktestResult& r) { return r.winRate; };
    const auto exposure = [](const BacktestResult& r) { return r.exposure; };
    std::cout << "| **Average** | " << percent(meanOf(strategy, cagr)) << " | "
              << fixed2(meanSharpe(strategy)) << " | " << percent(meanOf(strategy, drawdown))
              << " | " << fixed2(meanOf(strategy, trades)) << " | "
              << percent(meanOf(strategy, winRate)) << " | "
              << percent(meanOf(strategy, exposure)) << " | "
              << percent(meanOf(buyAndHold, cagr)) << " | " << fixed2(meanSharpe(buyAndHold))
              << " | " << percent(meanOf(buyAndHold, drawdown)) << " |\n";
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

std::vector<double> dailyReturns(const std::vector<double>& equity) {
    std::vector<double> returns;
    for (size_t i = 1; i < equity.size(); ++i) {
        returns.push_back(equity[i] / equity[i - 1] - 1.0);
    }
    return returns;
}

// Everything the study produces for one family.
struct FamilyResults {
    const Family* family = nullptr;
    Params chosen;
    double chosenInSampleSharpe = 0.0;
    double chosenOutOfSampleSharpe = 0.0;
    size_t outOfSampleRank = 0;
    double bestOutOfSampleSharpe = 0.0;
    double medianOutOfSampleSharpe = 0.0;
    size_t positiveOutOfSample = 0;
    size_t beatBuyAndHoldOutOfSample = 0;
    std::vector<Row> inSample;     // chosen parameters, one row per symbol
    std::vector<Row> outOfSample;  // chosen parameters, one row per symbol
};

FamilyResults studyFamily(const Family& family, const std::vector<PriceHistory>& universe,
                          const std::vector<WindowData>& inSample,
                          const std::vector<WindowData>& outOfSample,
                          double buyAndHoldOutOfSampleSharpe, const fs::path& resultsDir) {
    const size_t symbols = universe.size();
    std::vector<Row> inSampleGrid;
    std::vector<Row> outOfSampleGrid;
    std::vector<double> inSampleSharpe;
    std::vector<double> outOfSampleSharpe;

    for (const Params& params : family.grid) {
        const std::unique_ptr<Strategy> strategy = family.make(params);
        const std::string label = paramLabel(family, params);
        double inSum = 0.0;
        double outSum = 0.0;
        for (size_t s = 0; s < symbols; ++s) {
            const std::string& symbol = universe[s].getAssetSymbol();
            BacktestResult in = runStrategy(*strategy, inSample[s]);
            BacktestResult out = runStrategy(*strategy, outOfSample[s]);
            inSum += in.sharpeRatio;
            outSum += out.sharpeRatio;
            inSampleGrid.push_back({IN_SAMPLE.name, family.name, symbol, label, std::move(in)});
            outOfSampleGrid.push_back(
                {OUT_OF_SAMPLE.name, family.name, symbol, label, std::move(out)});
        }
        inSampleSharpe.push_back(inSum / static_cast<double>(symbols));
        outOfSampleSharpe.push_back(outSum / static_cast<double>(symbols));
    }

    writeGridCsv(resultsDir / (family.fileStem + "_in_sample_grid.csv"), family, family.grid,
                 inSampleGrid, symbols);
    writeGridCsv(resultsDir / (family.fileStem + "_out_of_sample_grid.csv"), family,
                 family.grid, outOfSampleGrid, symbols);

    const size_t best = static_cast<size_t>(
        std::max_element(inSampleSharpe.begin(), inSampleSharpe.end()) -
        inSampleSharpe.begin());

    FamilyResults results;
    results.family = &family;
    results.chosen = family.grid[best];
    results.chosenInSampleSharpe = inSampleSharpe[best];
    results.chosenOutOfSampleSharpe = outOfSampleSharpe[best];
    const auto countAbove = [&](double threshold) {
        return static_cast<size_t>(
            std::count_if(outOfSampleSharpe.begin(), outOfSampleSharpe.end(),
                          [&](double sharpe) { return sharpe > threshold; }));
    };
    results.outOfSampleRank = 1 + countAbove(outOfSampleSharpe[best]);
    results.positiveOutOfSample = countAbove(0.0);
    results.beatBuyAndHoldOutOfSample = countAbove(buyAndHoldOutOfSampleSharpe);
    results.bestOutOfSampleSharpe =
        *std::max_element(outOfSampleSharpe.begin(), outOfSampleSharpe.end());
    std::vector<double> sorted = outOfSampleSharpe;
    std::sort(sorted.begin(), sorted.end());
    results.medianOutOfSampleSharpe =
        (sorted[(sorted.size() - 1) / 2] + sorted[sorted.size() / 2]) / 2.0;

    results.inSample.assign(inSampleGrid.begin() + static_cast<std::ptrdiff_t>(best * symbols),
                            inSampleGrid.begin() +
                                static_cast<std::ptrdiff_t>((best + 1) * symbols));
    results.outOfSample.assign(
        outOfSampleGrid.begin() + static_cast<std::ptrdiff_t>(best * symbols),
        outOfSampleGrid.begin() + static_cast<std::ptrdiff_t>((best + 1) * symbols));
    return results;
}

void printFamily(const FamilyResults& r, const std::vector<Row>& buyAndHoldIn,
                 const std::vector<Row>& buyAndHoldOut) {
    const Family& family = *r.family;
    const size_t gridSize = family.grid.size();
    std::cout << "\n## " << family.name << "\n\n"
              << "Chosen in-sample (" << IN_SAMPLE.start << " to " << IN_SAMPLE.end
              << "): " << paramLabel(family, r.chosen) << ", mean Sharpe "
              << fixed2(r.chosenInSampleSharpe) << " (best of " << gridSize << ").\n"
              << "Out-of-sample it ranks " << r.outOfSampleRank << " of " << gridSize
              << " parameter sets by mean Sharpe (" << fixed2(r.chosenOutOfSampleSharpe)
              << "; best " << fixed2(r.bestOutOfSampleSharpe) << ", median "
              << fixed2(r.medianOutOfSampleSharpe) << ").\n"
              << "Out-of-sample, " << r.positiveOutOfSample << " of " << gridSize
              << " parameter sets have a positive mean Sharpe and "
              << r.beatBuyAndHoldOutOfSample << " beat buy-and-hold's mean Sharpe of "
              << fixed2(meanSharpe(buyAndHoldOut)) << ".\n";

    printComparison(IN_SAMPLE.label + " " + IN_SAMPLE.start + " to " + IN_SAMPLE.end,
                    family.shortName, r.inSample, buyAndHoldIn);
    printComparison(OUT_OF_SAMPLE.label + " " + OUT_OF_SAMPLE.start + " to " + OUT_OF_SAMPLE.end,
                    family.shortName, r.outOfSample, buyAndHoldOut);
    printHoldingPeriods("in-sample", r.inSample);
    printHoldingPeriods("out-of-sample", r.outOfSample);
}

// Averages across symbols for every strategy side by side, one table per window.
void printSideBySide(const std::vector<FamilyResults>& families,
                     const std::vector<Row>& buyAndHoldIn, const std::vector<Row>& buyAndHoldOut) {
    std::cout << "\n## Comparison\n";
    for (const Window* window : {&IN_SAMPLE, &OUT_OF_SAMPLE}) {
        const bool in = window == &IN_SAMPLE;
        std::cout << "\n### " << window->label << " " << window->start << " to " << window->end
                  << ", average of " << buyAndHoldIn.size() << " symbols\n\n"
                  << "| Strategy | CAGR | Sharpe | Max DD | Trades | Win rate | In market |\n"
                  << "|---|---:|---:|---:|---:|---:|---:|\n";
        const auto printRow = [](const std::string& name, const std::vector<Row>& rows,
                                 bool withTrades) {
            const auto field = [&](auto member) {
                return meanOf(rows, [member](const BacktestResult& r) {
                    return static_cast<double>(r.*member);
                });
            };
            std::cout << "| " << name << " | " << percent(field(&BacktestResult::annualizedReturn))
                      << " | " << fixed2(field(&BacktestResult::sharpeRatio)) << " | "
                      << percent(field(&BacktestResult::maxDrawdown)) << " | "
                      << (withTrades ? fixed2(field(&BacktestResult::totalTrades)) : "1") << " | "
                      << (withTrades ? percent(field(&BacktestResult::winRate)) : "-") << " | "
                      << percent(field(&BacktestResult::exposure)) << " |\n";
        };
        for (const auto& f : families) {
            printRow(f.family->name + " (" + paramLabel(*f.family, f.chosen) + ")",
                     in ? f.inSample : f.outOfSample, true);
        }
        printRow("BuyAndHold", in ? buyAndHoldIn : buyAndHoldOut, false);
    }

    // Do the strategies make and lose money on the same days?
    for (size_t a = 0; a < families.size(); ++a) {
        for (size_t b = a + 1; b < families.size(); ++b) {
            std::cout << "\n### Correlation of daily returns, " << families[a].family->shortName
                      << " vs " << families[b].family->shortName << ", out-of-sample\n\n"
                      << "| Symbol | Correlation |\n|---|---:|\n";
            double sum = 0.0;
            const auto& rowsA = families[a].outOfSample;
            const auto& rowsB = families[b].outOfSample;
            for (size_t s = 0; s < rowsA.size(); ++s) {
                const double correlation =
                    calculateCorrelation(dailyReturns(rowsA[s].result.equityCurve),
                                         dailyReturns(rowsB[s].result.equityCurve));
                sum += correlation;
                std::cout << "| " << rowsA[s].symbol << " | " << fixed2(correlation) << " |\n";
            }
            std::cout << "| **Average** | " << fixed2(sum / static_cast<double>(rowsA.size()))
                      << " |\n";
        }
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

        // Benchmark
        std::vector<Row> buyAndHoldIn;
        std::vector<Row> buyAndHoldOut;
        BuyAndHoldStrategy buyAndHold;
        for (size_t s = 0; s < universe.size(); ++s) {
            const std::string& symbol = universe[s].getAssetSymbol();
            buyAndHoldIn.push_back({IN_SAMPLE.name, buyAndHold.name(), symbol, "",
                                    runStrategy(buyAndHold, inSample[s])});
            buyAndHoldOut.push_back({OUT_OF_SAMPLE.name, buyAndHold.name(), symbol, "",
                                     runStrategy(buyAndHold, outOfSample[s])});
        }

        // Each strategy family
        const std::vector<Family> families = studyFamilies();
        std::vector<FamilyResults> results;
        for (const auto& family : families) {
            results.push_back(studyFamily(family, universe, inSample, outOfSample,
                                          meanSharpe(buyAndHoldOut), resultsDir));
        }

        // Outputs: summary, trades and equity curves for the chosen parameters
        std::vector<Row> summary;
        for (const auto& r : results) {
            summary.insert(summary.end(), r.inSample.begin(), r.inSample.end());
            summary.insert(summary.end(), r.outOfSample.begin(), r.outOfSample.end());
        }
        summary.insert(summary.end(), buyAndHoldIn.begin(), buyAndHoldIn.end());
        summary.insert(summary.end(), buyAndHoldOut.begin(), buyAndHoldOut.end());
        writeSummaryCsv(resultsDir / "summary.csv", summary);

        std::ofstream trades = openOutput(resultsDir / "trades.csv");
        trades << "window,strategy,symbol,entry_date,exit_date,entry_price,exit_price,shares,"
                  "pnl,return\n";
        for (const auto& r : results) {
            for (const auto* rows : {&r.inSample, &r.outOfSample}) {
                for (const auto& row : *rows) {
                    for (const auto& t : row.result.trades) {
                        trades << row.window << "," << row.strategy << "," << t.symbol << ","
                               << t.entryDate << "," << t.exitDate << "," << t.entryPrice
                               << "," << t.exitPrice << "," << t.shares << "," << t.pnl << ","
                               << t.returnPct << "\n";
                    }
                }
            }
        }

        std::ofstream equity = openOutput(resultsDir / "equity_out_of_sample.csv");
        equity << "date,symbol,strategy,equity\n";
        std::vector<const std::vector<Row>*> equityRows;
        for (const auto& r : results) {
            equityRows.push_back(&r.outOfSample);
        }
        equityRows.push_back(&buyAndHoldOut);
        for (const auto* rows : equityRows) {
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

        // Report
        std::cout << "# Strategy study\n\n"
                  << "Symbols: " << universe.size() << ". Costs: " << COMMISSION_RATE * 100.0
                  << "% commission + " << SLIPPAGE_BPS
                  << " bps slippage per side. Fills at next open. No interest on cash.\n";
        for (const auto& r : results) {
            printFamily(r, buyAndHoldIn, buyAndHoldOut);
        }
        printSideBySide(results, buyAndHoldIn, buyAndHoldOut);
        std::cout << "\nCSV results written to " << resultsDir.string() << "/\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
