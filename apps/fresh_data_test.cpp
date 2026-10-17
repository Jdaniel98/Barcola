// Phase 7: the pre-registered test on fresh data. Every choice is fixed in
// apps/preregistration.h; the hypotheses and how they are judged are in
// docs/phase-7-preregistration.md.
//
// Usage:
//   fresh_data_test select <data_dir> <six|dow29>
//       Choose each strategy's parameters on the selection window (2022-2024) and print them.
//       Run before any test data existed; its output is recorded in preregistration.h.
//   fresh_data_test run <data_dir> <results_dir> <six|dow29>
//       Run the recorded strategies, unchanged, once on the test window (2025-01 to 2026-09).

#include <algorithm>
#include <exception>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "buy_and_hold_strategy.h"
#include "core/time_utils.h"
#include "preregistration.h"
#include "report/svg_chart.h"
#include "study_common.h"

using namespace barcola;
using namespace barcola::study;
using namespace barcola::phase7;

namespace {

constexpr double CLEAR_RESULT = 0.95;  // share of resamples needed to call a comparison

const std::vector<std::string>& universeSymbols(const std::string& universe) {
    if (universe == "six") {
        return SIX_SYMBOLS;
    }
    if (universe == "dow29") {
        return DOW29_SYMBOLS;
    }
    throw std::invalid_argument("Unknown universe '" + universe + "' (expected six or dow29)");
}

// Loads the data folder and checks it holds exactly the pre-registered symbols.
std::vector<PriceHistory> loadExactUniverse(const fs::path& dataDir, const std::string& universe) {
    std::vector<PriceHistory> histories = loadUniverse(dataDir);
    std::set<std::string> found;
    for (const auto& h : histories) {
        found.insert(h.getAssetSymbol());
    }
    const auto& expectedList = universeSymbols(universe);
    const std::set<std::string> expected(expectedList.begin(), expectedList.end());
    if (found != expected) {
        std::string message =
            dataDir.string() + " does not match the pre-registered " + universe + " universe.";
        for (const auto& s : expected) {
            if (!found.count(s))
                message += " Missing " + s + ".";
        }
        for (const auto& s : found) {
            if (!expected.count(s))
                message += " Unexpected " + s + ".";
        }
        throw std::runtime_error(message);
    }
    return histories;
}

const Params& chosenParams(const std::string& universe, const std::string& family) {
    for (const auto& c : CHOSEN) {
        if (c.universe == universe && c.family == family) {
            return c.params;
        }
    }
    throw std::runtime_error("No pre-registered parameters for " + family + " on " + universe);
}

std::string paramsInitializer(const Params& params) {
    std::string out = "{";
    for (size_t i = 0; i < params.size(); ++i) {
        std::ostringstream value;
        value << params[i];
        out += (i == 0 ? "" : ", ") + value.str();
    }
    return out + "}";
}

int select(const fs::path& dataDir, const std::string& universe) {
    const std::vector<PriceHistory> histories = loadExactUniverse(dataDir, universe);
    std::vector<WindowData> data;
    for (const auto& h : histories) {
        data.push_back(sliceWindow(h, SELECTION));
    }

    std::cout << "# Phase 7 parameter selection, " << universe << " (" << histories.size()
              << " symbols)\n\nRule: highest average Sharpe ratio across symbols on "
              << SELECTION.start << " to " << SELECTION.end << ".\n";
    std::string initializers;
    for (const auto& family : families()) {
        const Selection selection = chooseByMeanSharpe(family, data);
        std::vector<size_t> order(family.grid.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            return selection.meanSharpe[a] > selection.meanSharpe[b];
        });

        std::cout << "\n## " << family.displayName << "\n\n| Rank | Parameters | Mean Sharpe |\n"
                  << "|---:|---|---:|\n";
        for (size_t r = 0; r < std::min<size_t>(3, order.size()); ++r) {
            std::cout << "| " << r + 1 << " | " << paramLabel(family, family.grid[order[r]])
                      << " | " << fixed2(selection.meanSharpe[order[r]]) << " |\n";
        }
        initializers += "    {\"" + universe + "\", \"" + family.name + "\", " +
                        paramsInitializer(family.grid[selection.best]) + "},\n";

        // Anyone can rerun this to check the recorded choice
        for (const auto& c : CHOSEN) {
            if (c.universe == universe && c.family == family.name) {
                std::cout << "\nRecorded in preregistration.h: " << paramLabel(family, c.params)
                          << (c.params == family.grid[selection.best] ? " (matches)" : " (DIFFERS)")
                          << "\n";
            }
        }
    }
    std::cout << "\nFor preregistration.h:\n\n```cpp\n" << initializers << "```\n";
    return 0;
}

std::string verdict(double shareAhead, bool predictFirstAhead) {
    const double predicted = predictFirstAhead ? shareAhead : 1.0 - shareAhead;
    if (predicted >= CLEAR_RESULT)
        return "Supported";
    if (predicted <= 1.0 - CLEAR_RESULT)
        return "Contradicted";
    return "Inconclusive";
}

int run(const fs::path& dataDir, const fs::path& resultsDir, const std::string& universe) {
    const std::vector<PriceHistory> histories = loadExactUniverse(dataDir, universe);
    const time_t lastNeeded = convertDateToEpoch(TEST.end) - END_DATE_TOLERANCE;
    std::vector<WindowData> data;
    for (const auto& h : histories) {
        if (h.getDataPoints().back().getDate() < lastNeeded) {
            throw std::runtime_error(h.getAssetSymbol() + " data ends before " + TEST.end);
        }
        data.push_back(sliceWindow(h, TEST));
        if (data.back().warmupBars < MIN_WARMUP_BARS) {
            throw std::runtime_error(h.getAssetSymbol() + " has too little history before " +
                                     TEST.start);
        }
    }
    fs::create_directories(resultsDir / "charts");

    // Run every pre-registered strategy, and buy-and-hold, on the test window
    const std::vector<Family> fams = families();
    std::vector<std::vector<Row>> rowsByStrategy;
    std::vector<std::string> paramsByStrategy;
    for (const auto& family : fams) {
        const Params& params = chosenParams(universe, family.name);
        const std::unique_ptr<Strategy> strategy = family.make(params);
        std::vector<Row> rows;
        for (size_t s = 0; s < histories.size(); ++s) {
            rows.push_back({TEST.name, family.name, histories[s].getAssetSymbol(),
                            paramLabel(family, params), runStrategy(*strategy, data[s])});
        }
        rowsByStrategy.push_back(rows);
        paramsByStrategy.push_back(paramLabel(family, params));
    }
    BuyAndHoldStrategy buyAndHold;
    std::vector<Row> holdRows;
    for (size_t s = 0; s < histories.size(); ++s) {
        holdRows.push_back({TEST.name, buyAndHold.name(), histories[s].getAssetSymbol(), "",
                            runStrategy(buyAndHold, data[s])});
    }

    // Report
    std::cout << "# Phase 7: pre-registered test, " << universe << "\n\n"
              << histories.size() << " symbols, " << TEST.start << " to " << TEST.end
              << ", run once with the parameters recorded before the data was downloaded. Costs: "
              << COMMISSION_RATE * 100.0 << "% commission + " << SLIPPAGE_BPS
              << " bps slippage per side. Fills at next open. No interest on cash.\n\n"
              << "## Results, average of " << histories.size() << " symbols\n\n"
              << "| Strategy | Parameters | CAGR | Sharpe | Max DD | Trades | Win rate "
                 "| In market |\n"
              << "|---|---|---:|---:|---:|---:|---:|---:|\n";
    const auto printRow = [](const std::string& name, const std::string& params,
                             const std::vector<Row>& rows) {
        const auto field = [&](auto member) {
            return meanOf(
                rows, [member](const BacktestResult& r) { return static_cast<double>(r.*member); });
        };
        std::cout << "| " << name << " | " << params << " | "
                  << percent(field(&BacktestResult::annualizedReturn)) << " | "
                  << fixed2(field(&BacktestResult::sharpeRatio)) << " | "
                  << percent(field(&BacktestResult::maxDrawdown)) << " | "
                  << fixed2(field(&BacktestResult::totalTrades)) << " | "
                  << percent(field(&BacktestResult::winRate)) << " | "
                  << percent(field(&BacktestResult::exposure)) << " |\n";
    };
    for (size_t f = 0; f < fams.size(); ++f) {
        printRow(fams[f].displayName, paramsByStrategy[f], rowsByStrategy[f]);
    }
    printRow("Buy-and-hold", "", holdRows);

    // Pre-registered comparisons
    std::vector<ReturnSet> sets;
    for (size_t f = 0; f < fams.size(); ++f) {
        sets.push_back(returnsOf(fams[f].displayName, rowsByStrategy[f]));
    }
    sets.push_back(returnsOf("Buy-and-hold", holdRows));
    const std::vector<GapEstimate> gaps = computeBootstrap(sets);
    const auto find = [&](const std::string& first, const std::string& second) {
        for (const auto& g : gaps) {
            if (g.first == first && g.second == second)
                return g;
        }
        throw std::runtime_error("No comparison of " + first + " with " + second);
    };
    struct Hypothesis {
        const char* id;
        const char* claim;
        std::string first;
        std::string second;
        bool predictFirstAhead;
    };
    const std::vector<Hypothesis> hypotheses = {
        {"H1", "Mean reversion trails buy-and-hold", fams[0].displayName, "Buy-and-hold", false},
        {"H2", "MA crossover trails buy-and-hold", fams[1].displayName, "Buy-and-hold", false},
        {"H3", "The trend filter improves mean reversion", fams[2].displayName, fams[0].displayName,
         true},
        {"H4", "The time stop improves mean reversion", fams[3].displayName, fams[0].displayName,
         true},
    };
    std::cout << "\n## Pre-registered hypotheses\n\n"
              << "Average Sharpe gap and its 95% range from " << BOOTSTRAP_RESAMPLES
              << " block-bootstrap resamples (" << BOOTSTRAP_BLOCK_DAYS
              << "-day blocks). A hypothesis is supported if the predicted side wins in at least "
              << percent(CLEAR_RESULT) << " of resamples, contradicted if it loses in at least "
              << percent(CLEAR_RESULT) << ", and otherwise inconclusive.\n\n"
              << "| | Hypothesis | Comparison | Sharpe gap | 95% range | First ahead in "
                 "| Verdict |\n"
              << "|---|---|---|---:|---:|---:|---|\n";
    for (const auto& h : hypotheses) {
        const GapEstimate g = find(h.first, h.second);
        std::cout << "| " << h.id << " | " << h.claim << " | " << g.first << " minus " << g.second
                  << " | " << fixed2(g.gap) << " | " << fixed2(g.low) << " to " << fixed2(g.high)
                  << " | " << percent(g.shareAhead) << " | "
                  << verdict(g.shareAhead, h.predictFirstAhead) << " |\n";
    }
    printBootstrap("All comparisons (exploratory beyond H1-H4)", sets);

    // Per-symbol Sharpe ratios
    std::cout << "\n## Sharpe ratio by symbol\n\n| Symbol |";
    for (const auto& family : fams)
        std::cout << " " << family.shortName << " |";
    std::cout << " B&H |\n|---|";
    for (size_t f = 0; f <= fams.size(); ++f)
        std::cout << "---:|";
    std::cout << "\n";
    for (size_t s = 0; s < histories.size(); ++s) {
        std::cout << "| " << histories[s].getAssetSymbol() << " |";
        for (const auto& rows : rowsByStrategy) {
            std::cout << " " << fixed2(rows[s].result.sharpeRatio) << " |";
        }
        std::cout << " " << fixed2(holdRows[s].result.sharpeRatio) << " |\n";
    }

    // Outputs: metrics summary and a growth chart of the averaged accounts
    std::vector<Row> summary;
    for (const auto& rows : rowsByStrategy)
        summary.insert(summary.end(), rows.begin(), rows.end());
    summary.insert(summary.end(), holdRows.begin(), holdRows.end());
    writeSummaryCsv(resultsDir / "summary.csv", summary);

    LineChart growth;
    growth.title = "Growth of $100,000, " + TEST.start.substr(0, 7) + " to " +
                   TEST.end.substr(0, 7) + " (fresh data)";
    growth.subtitle = "Average of " + std::to_string(histories.size()) +
                      " $100,000 accounts, pre-registered parameters, after costs";
    const auto& points = data[0].bars.getDataPoints();
    for (size_t i = data[0].warmupBars; i < points.size(); ++i) {
        growth.dates.push_back(points[i].getDateString());
    }
    growth.format = ValueFormat::Currency;
    for (size_t f = 0; f < fams.size(); ++f) {
        growth.series.push_back(
            {fams[f].displayName, fams[f].color, averageEquity(rowsByStrategy[f])});
    }
    growth.series.push_back({"Buy-and-hold", SeriesColor::Benchmark, averageEquity(holdRows)});
    writeFile(resultsDir / "charts" / "growth.svg", renderLineChart(growth));

    std::cout << "\nResults written to " << resultsDir.string() << "/\n";
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "";
        if (mode == "select" && argc == 4) {
            return select(argv[2], argv[3]);
        }
        if (mode == "run" && argc == 5) {
            return run(argv[2], argv[3], argv[4]);
        }
        std::cerr << "usage: fresh_data_test select <data_dir> <six|dow29>\n"
                     "       fresh_data_test run <data_dir> <results_dir> <six|dow29>\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
