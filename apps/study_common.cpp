#include "study_common.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "analysis/bootstrap.h"
#include "core/time_utils.h"
#include "detail/window_stats.h"
#include "preregistration.h"

namespace barcola::study {

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
    return {history.between(points[windowIndex - warmup].getDate(), end), warmup, window.name};
}

namespace {

// The comparisons a family's strategy makes, from the run's parameters. Mean reversion when
// lookback is set, otherwise the crossover. Time stops compare whole bars, so they have no margin.
struct MarginRule {
    size_t lookback = 0;
    double entryZ = 0.0;
    double exitZ = 0.0;
    size_t trendWindow = 0;
    size_t shortWindow = 0;
    size_t longWindow = 0;
};

MarginRule marginRuleFor(const std::string& family, const Params& params) {
    MarginRule rule;
    if (family == "MACrossover") {
        rule.shortWindow = static_cast<size_t>(params[0]);
        rule.longWindow = static_cast<size_t>(params[1]);
    } else if (family == "MeanReversion" || family == "MeanReversionTrend" ||
               family == "MeanReversionTimeStop") {
        rule.lookback = static_cast<size_t>(params[0]);
        rule.entryZ = params[1];
        rule.exitZ = params[2];
        if (family == "MeanReversionTrend") {
            rule.trendWindow = phase7::TREND_WINDOW;
        }
    } else {
        throw std::runtime_error("No margin rule for the strategy family " + family);
    }
    return rule;
}

// Wraps a strategy and records its closest decision in each run. It computes the same values
// from the same window_stats.h expressions, and always returns the wrapped strategy's signal.
class MarginProbe final : public Strategy {
public:
    MarginProbe(std::unique_ptr<Strategy> inner, const MarginRule& rule, MarginReport& report,
                std::string family, std::string params)
        : inner_(std::move(inner)), rule_(rule), report_(report) {
        row_.program = report.program();
        row_.family = std::move(family);
        row_.params = std::move(params);
    }

    void startRun(const std::string& window, const std::string& symbol) {
        row_.window = window;
        row_.symbol = symbol;
        row_.margin = std::numeric_limits<double>::infinity();
    }

    void finishRun() {
        // Still infinite means no comparison ran, so there was no decision to flip
        if (std::isfinite(row_.margin)) {
            report_.add(row_);
        }
    }

    void reset() override { inner_->reset(); }

    Signal evaluate(const BarView& bars, Position position) override {
        if (rule_.lookback > 0) {
            recordMeanReversion(bars, position);
        } else {
            recordCrossover(bars);
        }
        return inner_->evaluate(bars, position);
    }

    [[nodiscard]] std::string name() const override { return inner_->name(); }

private:
    void note(double margin, const BarView& bars, const char* comparison) {
        if (margin < row_.margin) {
            row_.margin = margin;
            row_.date = bars.back().getDateString();
            row_.comparison = comparison;
        }
    }

    // Mirrors MeanReversionStrategy::evaluate: the same guard, and each comparison only on the
    // bars where the strategy makes it.
    void recordMeanReversion(const BarView& bars, Position position) {
        if (bars.size() <= std::max(rule_.lookback, rule_.trendWindow)) {
            return;
        }
        const double z = detail::zScore(bars, rule_.lookback);
        if (position == Position::LONG) {
            note(std::abs(z - rule_.exitZ), bars, "exit_z");
            return;
        }
        note(std::abs(z - (-rule_.entryZ)), bars, "entry_z");
        if (z < -rule_.entryZ && rule_.trendWindow > 0) {
            const double trend = detail::trailingMean(bars, rule_.trendWindow);
            note(std::abs(detail::relativeGap(bars.back().getClosing(), trend)), bars, "trend");
        }
    }

    // Mirrors MovingAverageCrossoverStrategy::evaluate
    void recordCrossover(const BarView& bars) {
        if (bars.size() < rule_.longWindow) {
            return;
        }
        const double shortMA = detail::trailingMean(bars, rule_.shortWindow);
        const double longMA = detail::trailingMean(bars, rule_.longWindow);
        note(std::abs(detail::relativeGap(shortMA, longMA)), bars, "crossover");
    }

    std::unique_ptr<Strategy> inner_;
    MarginRule rule_;
    MarginReport& report_;
    MarginRow row_;
};

const char* const MARGIN_COLUMNS =
    "program,window,family,symbol,params,smallest_margin,date,comparison";

// True if path is folder or anything under it, following symlinks.
bool isInsideFolder(const fs::path& path, const fs::path& folder) {
    const fs::path relative = fs::weakly_canonical(fs::absolute(path))
                                  .lexically_relative(fs::weakly_canonical(fs::absolute(folder)));
    return !relative.empty() && *relative.begin() != "..";
}

std::vector<MarginRow> readMarginRows(const fs::path& path) {
    std::vector<MarginRow> rows;
    std::ifstream in(path);
    std::string line;
    if (!std::getline(in, line)) {
        return rows;  // Empty or new file
    }
    if (line != MARGIN_COLUMNS) {
        throw std::runtime_error(path.string() + " is not a margin report; not overwriting it");
    }
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        MarginRow row;
        std::string margin;
        std::getline(fields, row.program, ',');
        std::getline(fields, row.window, ',');
        std::getline(fields, row.family, ',');
        std::getline(fields, row.symbol, ',');
        std::getline(fields, row.params, ',');
        std::getline(fields, margin, ',');
        std::getline(fields, row.date, ',');
        std::getline(fields, row.comparison, ',');
        if (row.comparison.empty()) {
            throw std::runtime_error(path.string() + " has a malformed row: " + line);
        }
        row.margin = std::stod(margin);
        rows.push_back(std::move(row));
    }
    return rows;
}

}  // namespace

BacktestResult runStrategy(Strategy& strategy, const WindowData& data, double costMultiplier) {
    BacktestConfig config;
    config.initialCapital = INITIAL_CAPITAL;
    config.commissionRate = COMMISSION_RATE * costMultiplier;
    config.slippageBps = SLIPPAGE_BPS * costMultiplier;
    config.positionFraction = 1.0;
    config.riskFreeRate = RISK_FREE_RATE;
    config.fillTiming = FillTiming::NEXT_OPEN;
    config.warmupBars = data.warmupBars;
    auto* const probe = dynamic_cast<MarginProbe*>(&strategy);
    if (probe != nullptr) {
        probe->startRun(data.window, data.bars.getAssetSymbol());
    }
    BacktestResult result = BacktestEngine(config).run(strategy, data.bars);
    if (probe != nullptr) {
        probe->finishRun();
    }
    return result;
}

MarginReport::MarginReport(std::string program, const fs::path& path, const fs::path& resultsDir)
    : program_(std::move(program)), path_(path) {
    if (isInsideFolder(path, fs::current_path() / "results") || isInsideFolder(path, resultsDir)) {
        throw std::runtime_error("The margin report must be outside the results folders: " +
                                 path.string());
    }
    readMarginRows(path_);  // Refuse a file that isn't a report before the study runs
    const std::ofstream writable(path_, std::ios::app);
    if (!writable) {
        throw std::runtime_error("Cannot write " + path_.string());
    }
}

void MarginReport::add(MarginRow row) {
    rows_.push_back(std::move(row));
}

const std::vector<MarginRow>& MarginReport::rows() const {
    return rows_;
}

const std::string& MarginReport::program() const {
    return program_;
}

void MarginReport::write() const {
    std::vector<MarginRow> all = readMarginRows(path_);
    all.insert(all.end(), rows_.begin(), rows_.end());
    std::stable_sort(all.begin(), all.end(),
                     [](const MarginRow& a, const MarginRow& b) { return a.margin < b.margin; });

    std::ofstream out(path_);
    if (!out) {
        throw std::runtime_error("Cannot write " + path_.string());
    }
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << MARGIN_COLUMNS << "\n";
    for (const MarginRow& row : all) {
        out << row.program << "," << row.window << "," << row.family << "," << row.symbol << ","
            << row.params << "," << row.margin << "," << row.date << "," << row.comparison << "\n";
    }

    std::map<std::string, const MarginRow*> smallest;
    for (const MarginRow& row : rows_) {
        const auto found = smallest.find(row.family);
        if (found == smallest.end() || row.margin < found->second->margin) {
            smallest[row.family] = &row;
        }
    }
    for (const auto& [family, row] : smallest) {
        std::cerr << "Smallest decision margin, " << family << ": " << row->margin << " ("
                  << row->comparison << ", " << row->symbol << " " << row->date << ", "
                  << row->window << ", " << row->params << ")\n";
    }
}

std::optional<MarginReport> marginReportFromEnvironment(const std::string& program,
                                                        const fs::path& resultsDir) {
    const char* const path = std::getenv("BARCOLA_MARGIN_REPORT");
    if (path == nullptr || *path == '\0') {
        return std::nullopt;
    }
    return MarginReport(program, path, resultsDir);
}

std::vector<Family> withMarginProbes(std::vector<Family> families, MarginReport* report) {
    if (report == nullptr) {
        return families;
    }
    for (Family& family : families) {
        family.make = [original = family,
                       report](const Params& params) -> std::unique_ptr<Strategy> {
            const MarginRule rule = marginRuleFor(original.name, params);
            return std::make_unique<MarginProbe>(original.make(params), rule, *report,
                                                 original.name, paramLabel(original, params));
        };
    }
    return families;
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

void writeFile(const fs::path& path, const std::string& contents) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Cannot write " + path.string());
    }
    out << contents;
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

std::vector<double> dailyReturns(const std::vector<double>& equity) {
    std::vector<double> returns;
    for (size_t i = 1; i < equity.size(); ++i) {
        returns.push_back(equity[i] / equity[i - 1] - 1.0);
    }
    return returns;
}

std::vector<double> averageEquity(const std::vector<Row>& rows) {
    std::vector<double> average(rows.front().result.equityCurve.size(), 0.0);
    for (const auto& row : rows) {
        const auto& curve = row.result.equityCurve;
        if (curve.size() != average.size()) {
            throw std::runtime_error(
                "Equity curves differ in length; symbols trade on "
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

std::vector<GapEstimate> computeBootstrap(const std::vector<ReturnSet>& sets) {
    const size_t days = sets.front().bySymbol.front().size();
    for (const auto& set : sets) {
        for (const auto& returns : set.bySymbol) {
            if (returns.size() != days) {
                throw std::runtime_error("Return series of different lengths for " + set.name);
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

    std::vector<GapEstimate> estimates;
    for (size_t p = 0; p < pairs.size(); ++p) {
        const auto& [a, b] = pairs[p];
        GapEstimate e;
        e.first = sets[a].name;
        e.second = sets[b].name;
        e.gap = actual[a] - actual[b];
        e.low = percentile(gaps[p], 0.025);
        e.high = percentile(gaps[p], 0.975);
        e.shareAhead = static_cast<double>(std::count_if(gaps[p].begin(), gaps[p].end(),
                                                         [](double gap) { return gap > 0.0; })) /
                       static_cast<double>(BOOTSTRAP_RESAMPLES);
        e.days = days;
        estimates.push_back(e);
    }
    return estimates;
}

void printBootstrap(const std::string& title, const std::vector<ReturnSet>& sets) {
    const std::vector<GapEstimate> estimates = computeBootstrap(sets);
    std::cout << "\n### " << title << "\n\n"
              << BOOTSTRAP_RESAMPLES << " resamples of " << estimates.front().days
              << " daily returns in " << BOOTSTRAP_BLOCK_DAYS
              << "-day blocks; average Sharpe across symbols.\n\n"
              << "| Comparison | Sharpe gap | 95% range | First ahead in |\n"
              << "|---|---:|---:|---:|\n";
    for (const auto& e : estimates) {
        std::cout << "| " << e.first << " minus " << e.second << " | " << fixed2(e.gap) << " | "
                  << fixed2(e.low) << " to " << fixed2(e.high) << " | " << percent(e.shareAhead)
                  << " |\n";
    }
}

Selection chooseByMeanSharpe(const Family& family, const std::vector<WindowData>& data) {
    Selection selection;
    for (size_t g = 0; g < family.grid.size(); ++g) {
        const std::unique_ptr<Strategy> strategy = family.make(family.grid[g]);
        double sum = 0.0;
        for (const auto& window : data) {
            sum += runStrategy(*strategy, window).sharpeRatio;
        }
        selection.meanSharpe.push_back(sum / static_cast<double>(data.size()));
        if (selection.meanSharpe[g] > selection.meanSharpe[selection.best]) {
            selection.best = g;
        }
    }
    return selection;
}

void writeMetrics(std::ostream& out, const BacktestResult& r) {
    out << r.totalReturn << "," << r.annualizedReturn << "," << r.sharpeRatio << ","
        << r.sortinoRatio << "," << r.maxDrawdown << "," << r.calmarRatio << "," << r.totalTrades
        << "," << r.winRate << "," << r.profitFactor << "," << r.avgHoldDays << "," << r.exposure
        << "\n";
}

void writeSummaryCsv(const fs::path& path, const std::vector<Row>& rows) {
    std::ofstream out = openOutput(path);
    out << "window,strategy,symbol,params," << METRIC_COLUMNS << "\n";
    for (const auto& row : rows) {
        out << row.window << "," << row.strategy << "," << row.symbol << "," << row.params << ",";
        writeMetrics(out, row.result);
    }
}

ReturnSet returnsOf(const std::string& name, const std::vector<Row>& rows) {
    ReturnSet set{name, {}};
    for (const auto& row : rows) {
        set.bySymbol.push_back(dailyReturns(row.result.equityCurve));
    }
    return set;
}

}  // namespace barcola::study
