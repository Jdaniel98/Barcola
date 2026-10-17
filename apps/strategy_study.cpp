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
#include "study_common.h"

namespace fs = std::filesystem;
using namespace barcola;
using namespace barcola::study;

namespace {

const Window IN_SAMPLE{"in_sample", "In-sample", "2014-01-01", "2019-12-31"};
const Window OUT_OF_SAMPLE{"out_of_sample", "Out-of-sample", "2020-01-01", "2024-12-31"};

// Walk-forward: each test year's parameters are chosen on the TRAIN_YEARS before it.
constexpr int FIRST_TEST_YEAR = 2016;
constexpr int LAST_TEST_YEAR = 2024;
constexpr int TRAIN_YEARS = 3;

// Charts list the symbols by name up to this many; beyond it they use the universe label.
constexpr size_t MAX_LISTED_SYMBOLS = 8;

std::vector<Family> studyFamilies() {
    Family meanReversion{"MeanReversion",
                         "Mean reversion",
                         "MR",
                         "mean_reversion",
                         SeriesColor::First,
                         {"lookback", "entry_z", "exit_z"},
                         {},
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

    Family crossover{"MACrossover",
                     "MA crossover",
                     "MA",
                     "ma_crossover",
                     SeriesColor::Second,
                     {"short_window", "long_window"},
                     {},
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
                  << s.totalTrades << " | " << percent(s.winRate) << " | " << percent(s.exposure)
                  << " | " << percent(bh.annualizedReturn) << " | " << fixed2(bh.sharpeRatio)
                  << " | " << percent(bh.maxDrawdown) << " |\n";
    }

    const auto cagr = [](const BacktestResult& r) { return r.annualizedReturn; };
    const auto drawdown = [](const BacktestResult& r) { return r.maxDrawdown; };
    const auto trades = [](const BacktestResult& r) { return static_cast<double>(r.totalTrades); };
    const auto winRate = [](const BacktestResult& r) { return r.winRate; };
    const auto exposure = [](const BacktestResult& r) { return r.exposure; };
    std::cout << "| **Average** | " << percent(meanOf(strategy, cagr)) << " | "
              << fixed2(meanSharpe(strategy)) << " | " << percent(meanOf(strategy, drawdown))
              << " | " << fixed2(meanOf(strategy, trades)) << " | "
              << percent(meanOf(strategy, winRate)) << " | " << percent(meanOf(strategy, exposure))
              << " | " << percent(meanOf(buyAndHold, cagr)) << " | "
              << fixed2(meanSharpe(buyAndHold)) << " | " << percent(meanOf(buyAndHold, drawdown))
              << " |\n";
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
    std::vector<Row> inSample;                   // chosen parameters, one row per symbol
    std::vector<Row> outOfSample;                // chosen parameters, one row per symbol
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
    writeGridCsv(resultsDir / (family.fileStem + "_out_of_sample_grid.csv"), family, family.grid,
                 outOfSampleGrid, symbols);

    const size_t best = static_cast<size_t>(
        std::max_element(inSampleSharpe.begin(), inSampleSharpe.end()) - inSampleSharpe.begin());

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

    results.inSample.assign(
        inSampleGrid.begin() + static_cast<std::ptrdiff_t>(best * symbols),
        inSampleGrid.begin() + static_cast<std::ptrdiff_t>((best + 1) * symbols));
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
              << " parameter sets have a positive mean Sharpe and " << r.beatBuyAndHoldOutOfSample
              << " beat buy-and-hold's mean Sharpe of " << fixed2(meanSharpe(buyAndHoldOut))
              << ".\n";

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
    const std::string accounts =
        "Average of " + std::to_string(universe.size()) + " $100,000 accounts" +
        (symbolList.empty() ? "" : " (" + symbolList + ")") + ", after costs";
    const std::string outPeriod =
        OUT_OF_SAMPLE.start.substr(0, 4) + "\xE2\x80\x93" + OUT_OF_SAMPLE.end.substr(0, 4);
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
    sharpe.subtitle = "Parameters chosen on " + inPeriod + ", then run unchanged on " + outPeriod +
                      ". Average of " + std::to_string(universe.size()) + " symbols.";
    sharpe.fromLabel = inPeriod + " (in sample)";
    sharpe.toLabel = outPeriod + " (out of sample)";
    for (const auto& f : families) {
        sharpe.rows.push_back({f.family->displayName, f.family->color, meanSharpe(f.inSample),
                               meanSharpe(f.outOfSample)});
    }
    sharpe.rows.push_back({"Buy-and-hold", SeriesColor::Benchmark, meanSharpe(buyAndHoldIn),
                           meanSharpe(buyAndHoldOut)});
    writeFile(chartsDir / "sharpe_in_vs_out.svg", renderDumbbellChart(sharpe));

    // 4. Every parameter set out of sample against buy-and-hold
    StripChart sensitivity;
    sensitivity.title = "Every parameter set, " + outPeriod + " (out of sample)";
    sensitivity.subtitle = "One dot per parameter set. Ringed: the set chosen on " + inPeriod + ".";
    sensitivity.axisLabel =
        "Average Sharpe ratio across " + std::to_string(universe.size()) + " symbols, " + outPeriod;
    for (const auto& f : families) {
        sensitivity.rows.push_back(
            {f.family->displayName + " (" + std::to_string(f.family->grid.size()) + " sets)",
             f.family->color, f.outOfSampleSharpeBySet, f.chosenIndex, "chosen"});
    }
    sensitivity.reference = ReferenceLine{"Buy-and-hold", meanSharpe(buyAndHoldOut)};
    writeFile(chartsDir / "parameter_sensitivity.svg", renderStripChart(sensitivity));
}

// --- Robustness ---

struct WalkForward {
    std::vector<int> years;
    std::vector<std::vector<std::string>> chosen;   // [family][year]
    std::vector<std::vector<double>> yearlySharpe;  // [strategy][year], benchmark last
    std::vector<ReturnSet> returns;                 // per strategy, benchmark last
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
            const size_t best = chooseByMeanSharpe(family, trainData).best;
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
                  << (s == benchmark
                          ? std::string("-")
                          : std::to_string(ahead) + " of " + std::to_string(wf.years.size()))
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
        std::cout << " " << r.family->shortName << " CAGR | " << r.family->shortName << " Sharpe |";
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
    const time_t lastNeeded = convertDateToEpoch(OUT_OF_SAMPLE.end) - END_DATE_TOLERANCE;
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
                               << t.entryDate << "," << t.exitDate << "," << t.entryPrice << ","
                               << t.exitPrice << "," << t.shares << "," << t.pnl << ","
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
                           << (*rows)[s].symbol << "," << (*rows)[s].strategy << "," << curve[k]
                           << "\n";
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
