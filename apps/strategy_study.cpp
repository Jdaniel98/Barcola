// Strategy study: each strategy family against buy-and-hold and against each other on
// daily prices.
//
// For every family, one parameter set is chosen on the in-sample window only (2014-2019),
// by the highest mean Sharpe ratio across all symbols, then run unchanged on the
// out-of-sample window (2020-2024). Each family's full parameter grid is also run
// out-of-sample, so the write-up can show how much the choice matters.
//
// Robustness checks follow: block-bootstrap intervals for the Sharpe gaps between
// strategies, a walk-forward test that re-chooses parameters every year on the three years
// before it (test years 2016-2024), and the out-of-sample results at 0-3x the trading costs.
//
// Usage: strategy_study [data_dir] [results_dir] [universe_label]
//   data_dir        folder of <SYMBOL>.csv files (default: data/market)
//   results_dir     folder for the outputs (default: results)
//   universe_label  how charts describe the symbols when there are too many to list,
//                   e.g. "Dow 30 as of 2014" (default: none)

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "analysis/bootstrap.h"
#include "analysis/correlation.h"
#include "backtest/backtest_engine.h"
#include "buy_and_hold_strategy.h"
#include "core/price_history.h"
#include "core/time_utils.h"
#include "ma_crossover_strategy.h"
#include "mean_reversion_strategy.h"
#include "report/svg_chart.h"

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
// The least history a run can start with: the longest lookback (200 days) plus one bar.
constexpr size_t MIN_WARMUP_BARS = 201;

// Walk-forward: each test year's parameters are chosen on the TRAIN_YEARS before it.
constexpr int FIRST_TEST_YEAR = 2016;
constexpr int LAST_TEST_YEAR = 2024;
constexpr int TRAIN_YEARS = 3;

// Moving-block bootstrap of daily returns, about one month per block
constexpr size_t BOOTSTRAP_BLOCK_DAYS = 21;
constexpr size_t BOOTSTRAP_RESAMPLES = 1000;
constexpr std::uint32_t BOOTSTRAP_SEED = 7;

// Charts list the symbols by name up to this many; beyond it they use the universe label.
constexpr size_t MAX_LISTED_SYMBOLS = 8;

constexpr double INITIAL_CAPITAL = 100000.0;
constexpr double COMMISSION_RATE = 0.001;  // 0.1% per side
constexpr double SLIPPAGE_BPS = 5.0;       // per side
constexpr double RISK_FREE_RATE = 0.0;     // no interest on cash and no Sharpe hurdle

using Params = std::vector<double>;

// A strategy with a parameter grid to choose from.
struct Family {
    std::string name;         // as written to the CSV files, e.g. "MeanReversion"
    std::string displayName;  // for charts, e.g. "Mean reversion"
    std::string shortName;    // table column prefix, e.g. "MR"
    std::string fileStem;     // grid file prefix, e.g. "mean_reversion"
    SeriesColor color;        // the same in every chart
    std::vector<std::string> paramNames;
    std::vector<Params> grid;
    std::function<std::unique_ptr<Strategy>(const Params&)> make;
};

std::vector<Family> studyFamilies() {
    Family meanReversion{"MeanReversion", "Mean reversion", "MR", "mean_reversion",
                         SeriesColor::First,
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

    Family crossover{"MACrossover", "MA crossover", "MA", "ma_crossover", SeriesColor::Second,
                     {"short_window", "long_window"}, {},
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
    if (warmup < MIN_WARMUP_BARS) {
        std::cerr << "warning: " << history.getAssetSymbol() << " has only " << warmup
                  << " warm-up bars before " << window.start << "\n";
    }
    return {history.between(points[windowIndex - warmup].getDate(), end), warmup};
}

// costMultiplier scales both commission and slippage, for the cost-sensitivity check.
BacktestResult runStrategy(Strategy& strategy, const WindowData& data,
                           double costMultiplier = 1.0) {
    BacktestConfig config;
    config.initialCapital = INITIAL_CAPITAL;
    config.commissionRate = COMMISSION_RATE * costMultiplier;
    config.slippageBps = SLIPPAGE_BPS * costMultiplier;
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
    std::vector<double> outOfSampleSharpeBySet;  // mean across symbols, in grid order
    size_t chosenIndex = 0;
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
    results.chosenIndex = best;
    results.outOfSampleSharpeBySet = outOfSampleSharpe;
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

// Equity of each symbol's account averaged day by day, i.e. six equal accounts held side by
// side. All symbols must share the same trading days.
std::vector<double> averageEquity(const std::vector<Row>& rows) {
    std::vector<double> average(rows.front().result.equityCurve.size(), 0.0);
    for (const auto& row : rows) {
        const auto& curve = row.result.equityCurve;
        if (curve.size() != average.size()) {
            throw std::runtime_error("Equity curves differ in length; symbols trade on "
                                     "different days");
        }
        for (size_t k = 0; k < curve.size(); ++k) {
            average[k] += curve[k] / static_cast<double>(rows.size());
        }
    }
    return average;
}

std::vector<double> drawdownSeries(const std::vector<double>& equity) {
    std::vector<double> drawdown;
    double peak = equity.front();
    for (double value : equity) {
        peak = std::max(peak, value);
        drawdown.push_back(value / peak - 1.0);
    }
    return drawdown;
}

void writeFile(const fs::path& path, const std::string& contents) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Cannot write " + path.string());
    }
    out << contents;
}

void writeCharts(const fs::path& chartsDir, const std::vector<PriceHistory>& universe,
                 const std::string& universeLabel, const std::vector<WindowData>& outOfSample,
                 const std::vector<FamilyResults>& families, const std::vector<Row>& buyAndHoldIn,
                 const std::vector<Row>& buyAndHoldOut) {
    fs::create_directories(chartsDir);

    // Trading days of the out-of-sample window, which every symbol must share
    const auto windowDates = [&](size_t s) {
        std::vector<std::string> dates;
        const auto& points = outOfSample[s].bars.getDataPoints();
        for (size_t i = outOfSample[s].warmupBars; i < points.size(); ++i) {
            dates.push_back(points[i].getDateString());
        }
        return dates;
    };
    const std::vector<std::string> dates = windowDates(0);
    for (size_t s = 1; s < universe.size(); ++s) {
        if (windowDates(s) != dates) {
            throw std::runtime_error(universe[s].getAssetSymbol() +
                                     " trades on different days from " +
                                     universe[0].getAssetSymbol());
        }
    }

    std::string symbolList;
    for (size_t s = 0; s < universe.size(); ++s) {
        symbolList += (s == 0 ? "" : ", ") + universe[s].getAssetSymbol();
    }
    if (universe.size() > MAX_LISTED_SYMBOLS) {
        symbolList = universeLabel.empty() ? "" : universeLabel;
    }
    const std::string accounts = "Average of " + std::to_string(universe.size()) +
                                 " $100,000 accounts" +
                                 (symbolList.empty() ? "" : " (" + symbolList + ")") +
                                 ", after costs";
    const std::string outPeriod = OUT_OF_SAMPLE.start.substr(0, 4) + "\xE2\x80\x93" +
                                  OUT_OF_SAMPLE.end.substr(0, 4);
    const std::string inPeriod =
        IN_SAMPLE.start.substr(0, 4) + "\xE2\x80\x93" + IN_SAMPLE.end.substr(0, 4);

    // 1-2. Growth and drawdown of the averaged accounts
    LineChart growth;
    growth.title = "Growth of $100,000, " + outPeriod + " (out of sample)";
    growth.subtitle = accounts;
    growth.dates = dates;
    growth.format = ValueFormat::Currency;
    PanelChart drawdown;
    drawdown.title = "Drawdown from peak, " + outPeriod + " (out of sample)";
    drawdown.subtitle = "The same averaged accounts. 0% means at a new high.";
    drawdown.dates = dates;
    for (const auto& f : families) {
        const std::vector<double> equity = averageEquity(f.outOfSample);
        growth.series.push_back({f.family->displayName, f.family->color, equity});
        drawdown.series.push_back({f.family->displayName, f.family->color, drawdownSeries(equity)});
    }
    const std::vector<double> holdEquity = averageEquity(buyAndHoldOut);
    growth.series.push_back({"Buy-and-hold", SeriesColor::Benchmark, holdEquity});
    drawdown.series.push_back({"Buy-and-hold", SeriesColor::Benchmark, drawdownSeries(holdEquity)});
    writeFile(chartsDir / "growth.svg", renderLineChart(growth));
    writeFile(chartsDir / "drawdown.svg", renderPanelChart(drawdown));

    // 3. Sharpe ratio in the selection period vs the test period
    DumbbellChart sharpe;
    sharpe.title = "Sharpe ratio: selection period vs test period";
    sharpe.subtitle = "Parameters chosen on " + inPeriod + ", then run unchanged on " +
                      outPeriod + ". Average of " + std::to_string(universe.size()) +
                      " symbols.";
    sharpe.fromLabel = inPeriod + " (in sample)";
    sharpe.toLabel = outPeriod + " (out of sample)";
    for (const auto& f : families) {
        sharpe.rows.push_back({f.family->displayName, f.family->color,
                               meanSharpe(f.inSample), meanSharpe(f.outOfSample)});
    }
    sharpe.rows.push_back({"Buy-and-hold", SeriesColor::Benchmark, meanSharpe(buyAndHoldIn),
                           meanSharpe(buyAndHoldOut)});
    writeFile(chartsDir / "sharpe_in_vs_out.svg", renderDumbbellChart(sharpe));

    // 4. Every parameter set out of sample against buy-and-hold
    StripChart sensitivity;
    sensitivity.title = "Every parameter set, " + outPeriod + " (out of sample)";
    sensitivity.subtitle = "One dot per parameter set. Ringed: the set chosen on " + inPeriod +
                           ".";
    sensitivity.axisLabel = "Average Sharpe ratio across " + std::to_string(universe.size()) +
                            " symbols, " + outPeriod;
    for (const auto& f : families) {
        sensitivity.rows.push_back({f.family->displayName + " (" +
                                        std::to_string(f.family->grid.size()) + " sets)",
                                    f.family->color, f.outOfSampleSharpeBySet, f.chosenIndex,
                                    "chosen"});
    }
    sensitivity.reference = ReferenceLine{"Buy-and-hold", meanSharpe(buyAndHoldOut)};
    writeFile(chartsDir / "parameter_sensitivity.svg", renderStripChart(sensitivity));
}

// --- Robustness ---

// Daily returns of one strategy, one series per symbol. All series must cover the same days.
struct ReturnSet {
    std::string name;
    std::vector<std::vector<double>> bySymbol;
};

double meanSharpeOfReturns(const std::vector<std::vector<double>>& bySymbol,
                           const std::vector<size_t>* indices) {
    double sum = 0.0;
    for (const auto& returns : bySymbol) {
        if (indices == nullptr) {
            sum += calculateSharpeRatioFromReturns(returns, RISK_FREE_RATE);
            continue;
        }
        std::vector<double> resampled;
        resampled.reserve(indices->size());
        for (size_t i : *indices) {
            resampled.push_back(returns[i]);
        }
        sum += calculateSharpeRatioFromReturns(resampled, RISK_FREE_RATE);
    }
    return sum / static_cast<double>(bySymbol.size());
}

// How much of each Sharpe gap could be luck: resample the days in month-long blocks (the
// same days for every symbol and strategy, keeping cross-asset correlation) and see how the
// gap between average Sharpe ratios varies. The last set is the benchmark.
void printBootstrap(const std::string& title, const std::vector<ReturnSet>& sets) {
    const size_t days = sets.front().bySymbol.front().size();
    for (const auto& set : sets) {
        for (const auto& returns : set.bySymbol) {
            if (returns.size() != days) {
                throw std::runtime_error("Return series of different lengths in " + title);
            }
        }
    }

    // Pairs to compare: every strategy against the benchmark, then strategies against each other
    std::vector<std::pair<size_t, size_t>> pairs;
    const size_t benchmark = sets.size() - 1;
    for (size_t a = 0; a < benchmark; ++a) {
        pairs.emplace_back(a, benchmark);
    }
    for (size_t a = 0; a < benchmark; ++a) {
        for (size_t b = a + 1; b < benchmark; ++b) {
            pairs.emplace_back(b, a);
        }
    }

    std::vector<double> actual;
    for (const auto& set : sets) {
        actual.push_back(meanSharpeOfReturns(set.bySymbol, nullptr));
    }
    std::vector<std::vector<double>> gaps(pairs.size());
    std::mt19937 generator(BOOTSTRAP_SEED);
    for (size_t rep = 0; rep < BOOTSTRAP_RESAMPLES; ++rep) {
        const std::vector<size_t> indices =
            blockBootstrapIndices(days, BOOTSTRAP_BLOCK_DAYS, generator);
        std::vector<double> sharpe;
        for (const auto& set : sets) {
            sharpe.push_back(meanSharpeOfReturns(set.bySymbol, &indices));
        }
        for (size_t p = 0; p < pairs.size(); ++p) {
            gaps[p].push_back(sharpe[pairs[p].first] - sharpe[pairs[p].second]);
        }
    }

    std::cout << "\n### " << title << "\n\n"
              << BOOTSTRAP_RESAMPLES << " resamples of " << days << " daily returns in "
              << BOOTSTRAP_BLOCK_DAYS << "-day blocks; average Sharpe across symbols.\n\n"
              << "| Comparison | Sharpe gap | 95% range | First ahead in |\n"
              << "|---|---:|---:|---:|\n";
    for (size_t p = 0; p < pairs.size(); ++p) {
        const auto& [a, b] = pairs[p];
        const double ahead =
            static_cast<double>(std::count_if(gaps[p].begin(), gaps[p].end(),
                                              [](double gap) { return gap > 0.0; })) /
            static_cast<double>(BOOTSTRAP_RESAMPLES);
        std::cout << "| " << sets[a].name << " minus " << sets[b].name << " | "
                  << fixed2(actual[a] - actual[b]) << " | " << fixed2(percentile(gaps[p], 0.025))
                  << " to " << fixed2(percentile(gaps[p], 0.975)) << " | " << percent(ahead)
                  << " |\n";
    }
}

ReturnSet returnsOf(const std::string& name, const std::vector<Row>& rows) {
    ReturnSet set{name, {}};
    for (const auto& row : rows) {
        set.bySymbol.push_back(dailyReturns(row.result.equityCurve));
    }
    return set;
}

struct WalkForward {
    std::vector<int> years;
    std::vector<std::vector<std::string>> chosen;       // [family][year]
    std::vector<std::vector<double>> yearlySharpe;      // [strategy][year], benchmark last
    std::vector<ReturnSet> returns;                     // per strategy, benchmark last
};

// Re-choose each family's parameters every year on the TRAIN_YEARS before it and run them
// on that year alone. Every year starts flat with fresh capital, so each strategy (and the
// benchmark) re-enters on the first day and closes out on the last day of every year.
WalkForward runWalkForward(const std::vector<Family>& families,
                           const std::vector<PriceHistory>& universe) {
    WalkForward wf;
    wf.chosen.resize(families.size());
    wf.yearlySharpe.resize(families.size() + 1);
    for (const auto& family : families) {
        wf.returns.push_back({family.displayName, {}});
    }
    wf.returns.push_back({"Buy-and-hold", {}});
    for (auto& set : wf.returns) {
        set.bySymbol.resize(universe.size());
    }

    for (int year = FIRST_TEST_YEAR; year <= LAST_TEST_YEAR; ++year) {
        wf.years.push_back(year);
        const Window train{"wf_train", "Walk-forward train",
                           std::to_string(year - TRAIN_YEARS) + "-01-01",
                           std::to_string(year - 1) + "-12-31"};
        const Window test{"wf_test", "Walk-forward test", std::to_string(year) + "-01-01",
                          std::to_string(year) + "-12-31"};
        std::vector<WindowData> trainData;
        std::vector<WindowData> testData;
        for (const auto& history : universe) {
            trainData.push_back(sliceWindow(history, train));
            testData.push_back(sliceWindow(history, test));
        }

        const auto runOnTest = [&](Strategy& strategy, size_t setIndex) {
            double sharpeSum = 0.0;
            for (size_t s = 0; s < universe.size(); ++s) {
                const BacktestResult result = runStrategy(strategy, testData[s]);
                sharpeSum += result.sharpeRatio;
                const std::vector<double> returns = dailyReturns(result.equityCurve);
                auto& stitched = wf.returns[setIndex].bySymbol[s];
                stitched.insert(stitched.end(), returns.begin(), returns.end());
            }
            wf.yearlySharpe[setIndex].push_back(sharpeSum / static_cast<double>(universe.size()));
        };

        for (size_t f = 0; f < families.size(); ++f) {
            const Family& family = families[f];
            size_t best = 0;
            double bestSharpe = 0.0;
            for (size_t g = 0; g < family.grid.size(); ++g) {
                const std::unique_ptr<Strategy> strategy = family.make(family.grid[g]);
                double sum = 0.0;
                for (const auto& data : trainData) {
                    sum += runStrategy(*strategy, data).sharpeRatio;
                }
                const double mean = sum / static_cast<double>(trainData.size());
                if (g == 0 || mean > bestSharpe) {
                    best = g;
                    bestSharpe = mean;
                }
            }
            wf.chosen[f].push_back(paramLabel(family, family.grid[best]));
            const std::unique_ptr<Strategy> chosen = family.make(family.grid[best]);
            runOnTest(*chosen, f);
        }
        BuyAndHoldStrategy buyAndHold;
        runOnTest(buyAndHold, families.size());
    }
    return wf;
}

struct StitchedMetrics {
    double cagr = 0.0;
    double sharpe = 0.0;
    double maxDrawdown = 0.0;
};

// Averages across symbols of each symbol's compounded walk-forward record.
StitchedMetrics stitchedMetrics(const ReturnSet& set) {
    StitchedMetrics m;
    for (const auto& returns : set.bySymbol) {
        std::vector<double> equity = {INITIAL_CAPITAL};
        for (double r : returns) {
            equity.push_back(equity.back() * (1.0 + r));
        }
        m.cagr += annualizeReturn(equity.back() / INITIAL_CAPITAL - 1.0, returns.size());
        m.sharpe += calculateSharpeRatioFromReturns(returns, RISK_FREE_RATE);
        m.maxDrawdown += calculateMaxDrawdown(equity);
    }
    const double n = static_cast<double>(set.bySymbol.size());
    m.cagr /= n;
    m.sharpe /= n;
    m.maxDrawdown /= n;
    return m;
}

void printWalkForward(const std::vector<Family>& families, const WalkForward& wf) {
    const size_t benchmark = families.size();
    std::cout << "\n### Walk-forward, " << wf.years.front() << " to " << wf.years.back()
              << "\n\nEach year's parameters are chosen on the " << TRAIN_YEARS
              << " years before it, by average Sharpe across symbols, then run on that year "
                 "alone.\n\n| Year |";
    for (const auto& family : families) {
        std::cout << " " << family.shortName << " chosen | " << family.shortName << " Sharpe |";
    }
    std::cout << " B&H Sharpe |\n|---|";
    for (size_t f = 0; f < families.size(); ++f) {
        std::cout << "---|---:|";
    }
    std::cout << "---:|\n";
    for (size_t y = 0; y < wf.years.size(); ++y) {
        std::cout << "| " << wf.years[y] << " |";
        for (size_t f = 0; f < families.size(); ++f) {
            std::cout << " " << wf.chosen[f][y] << " | " << fixed2(wf.yearlySharpe[f][y]) << " |";
        }
        std::cout << " " << fixed2(wf.yearlySharpe[benchmark][y]) << " |\n";
    }

    std::cout << "\n| Strategy, all test years compounded | CAGR | Sharpe | Max DD "
                 "| Years ahead of B&H |\n|---|---:|---:|---:|---:|\n";
    for (size_t s = 0; s < wf.returns.size(); ++s) {
        const StitchedMetrics m = stitchedMetrics(wf.returns[s]);
        size_t ahead = 0;
        for (size_t y = 0; y < wf.years.size(); ++y) {
            ahead += wf.yearlySharpe[s][y] > wf.yearlySharpe[benchmark][y] ? 1 : 0;
        }
        std::cout << "| " << wf.returns[s].name << " | " << percent(m.cagr) << " | "
                  << fixed2(m.sharpe) << " | " << percent(m.maxDrawdown) << " | "
                  << (s == benchmark ? std::string("-")
                                     : std::to_string(ahead) + " of " +
                                           std::to_string(wf.years.size()))
                  << " |\n";
    }
}

// The out-of-sample results with every strategy's chosen parameters, at several multiples of
// the study's trading costs.
void printCostSensitivity(const std::vector<FamilyResults>& results,
                          const std::vector<WindowData>& outOfSample) {
    std::cout << "\n### Cost sensitivity, " << OUT_OF_SAMPLE.start.substr(0, 4) << " to "
              << OUT_OF_SAMPLE.end.substr(0, 4)
              << "\n\nThe chosen parameters at multiples of the study's costs (1x = "
              << COMMISSION_RATE * 100.0 << "% commission + " << SLIPPAGE_BPS
              << " bps slippage per side). Average of " << outOfSample.size()
              << " symbols.\n\n| Costs |";
    for (const auto& r : results) {
        std::cout << " " << r.family->shortName << " CAGR | " << r.family->shortName
                  << " Sharpe |";
    }
    std::cout << " B&H CAGR | B&H Sharpe |\n|---|";
    for (size_t f = 0; f <= results.size(); ++f) {
        std::cout << "---:|---:|";
    }
    std::cout << "\n";

    for (double multiplier : {0.0, 1.0, 2.0, 3.0}) {
        std::cout << "| " << multiplier << "x |";
        const auto printAverages = [&](Strategy& strategy) {
            double cagr = 0.0;
            double sharpe = 0.0;
            for (const auto& data : outOfSample) {
                const BacktestResult result = runStrategy(strategy, data, multiplier);
                cagr += result.annualizedReturn;
                sharpe += result.sharpeRatio;
            }
            const double n = static_cast<double>(outOfSample.size());
            std::cout << " " << percent(cagr / n) << " | " << fixed2(sharpe / n) << " |";
        };
        for (const auto& r : results) {
            const std::unique_ptr<Strategy> strategy = r.family->make(r.chosen);
            printAverages(*strategy);
        }
        BuyAndHoldStrategy buyAndHold;
        printAverages(buyAndHold);
        std::cout << "\n";
    }
}

// Why a symbol can't take part: its history must reach back far enough to warm up the first
// walk-forward training window and forward to the end of the out-of-sample window.
std::string coverageProblem(const PriceHistory& history) {
    const auto& points = history.getDataPoints();
    const time_t firstNeeded =
        convertDateToEpoch(std::to_string(FIRST_TEST_YEAR - TRAIN_YEARS) + "-01-01");
    const time_t lastNeeded = convertDateToEpoch(OUT_OF_SAMPLE.end) - 7 * 86400;
    const size_t barsBefore = static_cast<size_t>(
        std::lower_bound(points.begin(), points.end(), firstNeeded,
                         [](const PricePoint& p, time_t value) { return p.getDate() < value; }) -
        points.begin());
    if (points.empty() || barsBefore < MIN_WARMUP_BARS || points.back().getDate() < lastNeeded) {
        const std::string range = points.empty() ? "no data"
                                                 : points.front().getDateString() + " to " +
                                                       points.back().getDateString();
        return "history " + range + " does not cover " +
               std::to_string(FIRST_TEST_YEAR - TRAIN_YEARS) + " (with warm-up) to " +
               OUT_OF_SAMPLE.end;
    }
    return "";
}

void writeWalkForwardChart(const fs::path& chartsDir, const std::vector<Family>& families,
                           const WalkForward& wf, size_t symbols) {
    LineChart chart;
    chart.title = "Walk-forward: average Sharpe ratio in each test year";
    chart.subtitle = "Parameters re-chosen every year on the " + std::to_string(TRAIN_YEARS) +
                     " years before it. Average of " + std::to_string(symbols) + " symbols.";
    for (int year : wf.years) {
        chart.dates.push_back(std::to_string(year) + "-07-01");
    }
    chart.format = ValueFormat::Decimal;
    for (size_t f = 0; f < families.size(); ++f) {
        chart.series.push_back({families[f].displayName, families[f].color, wf.yearlySharpe[f]});
    }
    chart.series.push_back({"Buy-and-hold", SeriesColor::Benchmark, wf.yearlySharpe.back()});
    writeFile(chartsDir / "walk_forward.svg", renderLineChart(chart));
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const fs::path dataDir = argc > 1 ? argv[1] : "data/market";
        const fs::path resultsDir = argc > 2 ? argv[2] : "results";
        const std::string universeLabel = argc > 3 ? argv[3] : "";
        fs::create_directories(resultsDir);

        // Only symbols whose history covers the whole study take part
        std::vector<PriceHistory> universe;
        std::vector<std::string> excluded;
        for (const auto& history : loadUniverse(dataDir)) {
            const std::string problem = coverageProblem(history);
            if (problem.empty()) {
                universe.push_back(history);
            } else {
                excluded.push_back(history.getAssetSymbol() + ": " + problem);
            }
        }
        if (universe.empty()) {
            throw std::runtime_error("No symbol in " + dataDir.string() +
                                     " covers the study period");
        }
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
        for (const auto& reason : excluded) {
            std::cout << "\nExcluded " << reason << ".\n";
        }
        for (const auto& r : results) {
            printFamily(r, buyAndHoldIn, buyAndHoldOut);
        }
        printSideBySide(results, buyAndHoldIn, buyAndHoldOut);

        std::cout << "\n## Robustness\n";
        std::vector<ReturnSet> outOfSampleReturns;
        for (const auto& r : results) {
            outOfSampleReturns.push_back(returnsOf(r.family->displayName, r.outOfSample));
        }
        outOfSampleReturns.push_back(returnsOf("Buy-and-hold", buyAndHoldOut));
        printBootstrap("Confidence intervals, " + OUT_OF_SAMPLE.start.substr(0, 4) + " to " +
                           OUT_OF_SAMPLE.end.substr(0, 4),
                       outOfSampleReturns);
        const WalkForward walkForward = runWalkForward(families, universe);
        printWalkForward(families, walkForward);
        printBootstrap("Confidence intervals, walk-forward " + std::to_string(FIRST_TEST_YEAR) +
                           " to " + std::to_string(LAST_TEST_YEAR),
                       walkForward.returns);
        printCostSensitivity(results, outOfSample);

        writeCharts(resultsDir / "charts", universe, universeLabel, outOfSample, results,
                    buyAndHoldIn, buyAndHoldOut);
        writeWalkForwardChart(resultsDir / "charts", families, walkForward, universe.size());
        std::cout << "\nCSV results written to " << resultsDir.string() << "/\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
