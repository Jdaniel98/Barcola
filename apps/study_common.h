#pragma once

// Pieces shared by the study programs in apps/: windows of price history, running a strategy
// with the study's costs, parameter grids, report formatting and the bootstrap comparison.

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "backtest/backtest_engine.h"
#include "core/price_history.h"
#include "report/svg_chart.h"

namespace barcola::study {

namespace fs = std::filesystem;

// One year of history before each window. Every run gets the same history, so all
// parameter sets trade over identical days; the longest lookback (200 days) fits inside it.
inline constexpr size_t WARMUP_BARS = 252;

// The least history a run can start with: the longest lookback (200 days) plus one bar.
inline constexpr size_t MIN_WARMUP_BARS = 201;

// Moving-block bootstrap of daily returns, about one month per block
inline constexpr size_t BOOTSTRAP_BLOCK_DAYS = 21;
inline constexpr size_t BOOTSTRAP_RESAMPLES = 1000;
inline constexpr std::uint32_t BOOTSTRAP_SEED = 7;

inline constexpr double INITIAL_CAPITAL = 100000.0;
inline constexpr double COMMISSION_RATE = 0.001;  // 0.1% per side
inline constexpr double SLIPPAGE_BPS = 5.0;       // per side
inline constexpr double RISK_FREE_RATE = 0.0;     // no interest on cash and no Sharpe hurdle

// A symbol's data may end up to a week before a window's last date (weekends and holidays).
inline constexpr time_t END_DATE_TOLERANCE = time_t{7} * 24 * 60 * 60;

struct Window {
    std::string name;
    std::string label;
    std::string start;
    std::string end;
};

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

// Daily returns of one strategy, one series per symbol. All series must cover the same days.
struct ReturnSet {
    std::string name;
    std::vector<std::vector<double>> bySymbol;
};

WindowData sliceWindow(const PriceHistory& history, const Window& window);

// costMultiplier scales both commission and slippage, for the cost-sensitivity check.
BacktestResult runStrategy(Strategy& strategy, const WindowData& data,
                           double costMultiplier = 1.0);

std::string paramLabel(const Family& family, const Params& params);

std::vector<PriceHistory> loadUniverse(const fs::path& dataDir);

std::ofstream openOutput(const fs::path& path);

void writeFile(const fs::path& path, const std::string& contents);

std::string percent(double value);

std::string fixed2(double value);

double meanOf(const std::vector<Row>& rows,
              const std::function<double(const BacktestResult&)>& field);

double meanSharpe(const std::vector<Row>& rows);

std::vector<double> dailyReturns(const std::vector<double>& equity);

// Equity of each symbol's account averaged day by day, i.e. six equal accounts held side by
// side. All symbols must share the same trading days.
std::vector<double> averageEquity(const std::vector<Row>& rows);

std::vector<double> drawdownSeries(const std::vector<double>& equity);

double meanSharpeOfReturns(const std::vector<std::vector<double>>& bySymbol,
                           const std::vector<size_t>* indices);

// How much of each Sharpe gap could be luck: resample the days in month-long blocks (the
// same days for every symbol and strategy, keeping cross-asset correlation) and see how the
// gap between average Sharpe ratios varies. The last set is the benchmark.
void printBootstrap(const std::string& title, const std::vector<ReturnSet>& sets);

// The grid's parameter set with the highest average Sharpe ratio across symbols on the given
// windows. Ties go to the earlier set in the grid.
struct Selection {
    size_t best = 0;
    std::vector<double> meanSharpe;  // per parameter set, in grid order
};
Selection chooseByMeanSharpe(const Family& family, const std::vector<WindowData>& data);

// Comparison of two strategies' average Sharpe ratios under the block bootstrap.
struct GapEstimate {
    std::string first;
    std::string second;
    double gap = 0.0;         // first minus second, on the actual data
    double low = 0.0;         // 2.5th percentile of the resampled gap
    double high = 0.0;        // 97.5th percentile
    double shareAhead = 0.0;  // share of resamples where first beat second
    size_t days = 0;
};

// Every strategy against the benchmark (the last set), then each pair of strategies.
std::vector<GapEstimate> computeBootstrap(const std::vector<ReturnSet>& sets);

inline const char* const METRIC_COLUMNS =
    "total_return,cagr,sharpe,sortino,max_drawdown,calmar,trades,win_rate,profit_factor,"
    "avg_hold_days,exposure";

// Metrics columns, comma-separated, for one result.
void writeMetrics(std::ostream& out, const BacktestResult& r);

// One row per strategy, symbol and window, with a single params column.
void writeSummaryCsv(const fs::path& path, const std::vector<Row>& rows);

// Daily returns of each row's equity curve, one series per symbol.
ReturnSet returnsOf(const std::string& name, const std::vector<Row>& rows);

}  // namespace barcola::study
