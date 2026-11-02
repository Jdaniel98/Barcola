#include <gtest/gtest.h>
#include <unistd.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/time_utils.h"
#include "ma_crossover_strategy.h"
#include "mean_reversion_strategy.h"
#include "preregistration.h"
#include "study_common.h"

using namespace barcola;
using namespace barcola::study;

namespace {

// One bar per day from 2023-01-01, with open == close
WindowData makeWindow(const std::vector<double>& closes, size_t warmupBars) {
    PriceHistory history("TEST");
    const time_t start = convertDateToEpoch("2023-01-01");
    for (size_t i = 0; i < closes.size(); ++i) {
        history.addDataPoint(PricePoint(start + static_cast<time_t>(i) * 86400, closes[i]));
    }
    return {history, warmupBars, "test_window"};
}

Family meanReversionFamily(const std::string& name, MeanReversionOptions options = {}) {
    Family family{};
    family.name = name;
    family.paramNames = {"lookback", "entry_z", "exit_z"};
    family.make = [options](const Params& p) -> std::unique_ptr<Strategy> {
        return std::make_unique<MeanReversionStrategy>(static_cast<size_t>(p[0]), p[1], p[2],
                                                       options);
    };
    return family;
}

Family crossoverFamily() {
    Family family{};
    family.name = "MACrossover";
    family.paramNames = {"short_window", "long_window"};
    family.make = [](const Params& p) -> std::unique_ptr<Strategy> {
        return std::make_unique<MovingAverageCrossoverStrategy>(static_cast<size_t>(p[0]),
                                                                static_cast<size_t>(p[1]));
    };
    return family;
}

// A folder of its own for each test, removed afterwards
class StudyCommonTest : public ::testing::Test {
protected:
    void SetUp() override {
        const std::string test = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        dir_ = fs::temp_directory_path() /
               ("barcola_study_common_" + std::to_string(getpid()) + "_" + test);
        fs::create_directories(dir_);
    }
    void TearDown() override {
        std::error_code ignored;
        fs::remove_all(dir_, ignored);
    }

    // Runs one strategy of the family under a probe and returns the report's only row.
    MarginRow marginOf(const Family& family, const Params& params, const WindowData& data) {
        MarginReport report("test", dir_ / "margins.csv", dir_ / "results");
        const std::vector<Family> probed = withMarginProbes({family}, &report);
        const std::unique_ptr<Strategy> strategy = probed[0].make(params);
        runStrategy(*strategy, data);
        EXPECT_EQ(report.rows().size(), 1U);
        return report.rows().at(0);
    }

    fs::path dir_;
};

std::vector<std::string> linesOf(const fs::path& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

}  // namespace

TEST_F(StudyCommonTest, MeanReversion_Flat_MarginIsDistanceFromEntryThreshold) {
    // Bar 3: {10, 10, 13} gives z = sqrt(2), margin 1 + sqrt(2).
    // Bar 4: {10, 13, 12} has mean 35/3 and standard deviation sqrt(14)/3, so z = 1/sqrt(14),
    // margin 1 + 1/sqrt(14), the smaller of the two.
    const MarginRow row = marginOf(meanReversionFamily("MeanReversion"), {3, 1.0, 0.0},
                                   makeWindow({10, 10, 10, 13, 12}, 3));
    EXPECT_NEAR(row.margin, 1.0 + 1.0 / std::sqrt(14.0), 1e-10);
    EXPECT_EQ(row.comparison, "entry_z");
    EXPECT_EQ(row.date, "2023-01-05");
    EXPECT_EQ(row.program, "test");
    EXPECT_EQ(row.window, "test_window");
    EXPECT_EQ(row.family, "MeanReversion");
    EXPECT_EQ(row.symbol, "TEST");
    EXPECT_EQ(row.params, "lookback=3 entry_z=1 exit_z=0");
}

TEST_F(StudyCommonTest, MeanReversion_Long_MarginIsDistanceFromExitThreshold) {
    // Bar 3: {10, 10, 7} gives z = -sqrt(2), so it buys with margin sqrt(2) - 1.
    // Bar 4, now long: {10, 7, 8} gives z = -1/sqrt(14), margin 1/sqrt(14) from an exit z of 0.
    const MarginRow row = marginOf(meanReversionFamily("MeanReversion"), {3, 1.0, 0.0},
                                   makeWindow({10, 10, 10, 7, 8}, 3));
    EXPECT_NEAR(row.margin, 1.0 / std::sqrt(14.0), 1e-10);
    EXPECT_EQ(row.comparison, "exit_z");
    EXPECT_EQ(row.date, "2023-01-05");
}

TEST_F(StudyCommonTest, MeanReversion_ExitThresholdIsNotZero_UsesIt) {
    // Same bars as above, with z = -1/sqrt(14) now measured from an exit z of 0.5
    const MarginRow row = marginOf(meanReversionFamily("MeanReversion"), {3, 1.0, 0.5},
                                   makeWindow({10, 10, 10, 7, 8}, 3));
    EXPECT_NEAR(row.margin, std::sqrt(2.0) - 1.0, 1e-10);  // 0.5 + 1/sqrt(14) is larger
    EXPECT_EQ(row.comparison, "entry_z");
}

TEST_F(StudyCommonTest, TimeStop_HasTheSameMarginsAsPlainMeanReversion) {
    const MarginRow row = marginOf(meanReversionFamily("MeanReversionTimeStop", {0, 1}),
                                   {3, 1.0, 0.0}, makeWindow({10, 10, 10, 7, 8}, 3));
    EXPECT_NEAR(row.margin, 1.0 / std::sqrt(14.0), 1e-10);
    EXPECT_EQ(row.comparison, "exit_z");
}

TEST_F(StudyCommonTest, TrendFilter_MarginIsRelativeDistanceFromTrend) {
    // 200 bars at 100, then 90. z on {100, 100, 90} is -sqrt(2), margin sqrt(2) - 1 from the
    // entry threshold. The last 200 closes average 99.95, a gap of 9.95 / 99.95, which is smaller.
    std::vector<double> closes(200, 100.0);
    closes.push_back(90.0);
    const MarginRow row =
        marginOf(meanReversionFamily("MeanReversionTrend", {phase7::TREND_WINDOW, 0}),
                 {3, 1.0, 0.0}, makeWindow(closes, 200));
    EXPECT_NEAR(row.margin, 9.95 / 99.95, 1e-10);
    EXPECT_EQ(row.comparison, "trend");
}

TEST_F(StudyCommonTest, TrendFilter_OnlyCountsWhenTheStrategyComparesIt) {
    // The close sits exactly on its 200-day average, but z never reaches the entry threshold,
    // so the strategy never compares the two. z = 0, margin 1 from the entry threshold.
    const std::vector<double> closes(201, 100.0);
    const MarginRow row =
        marginOf(meanReversionFamily("MeanReversionTrend", {phase7::TREND_WINDOW, 0}),
                 {3, 1.0, 0.0}, makeWindow(closes, 200));
    EXPECT_DOUBLE_EQ(row.margin, 1.0);
    EXPECT_EQ(row.comparison, "entry_z");
}

TEST_F(StudyCommonTest, Crossover_MarginIsRelativeGapBetweenAverages) {
    // Short average 13.5, long average 38/3, so (5/6) / (38/3) = 5/76
    const MarginRow row = marginOf(crossoverFamily(), {2, 3}, makeWindow({10, 11, 12, 15}, 3));
    EXPECT_NEAR(row.margin, 5.0 / 76.0, 1e-10);
    EXPECT_EQ(row.comparison, "crossover");
    EXPECT_EQ(row.params, "short_window=2 long_window=3");
}

TEST_F(StudyCommonTest, Crossover_ExactTie_MarginIsZero) {
    const MarginRow row = marginOf(crossoverFamily(), {2, 3}, makeWindow({7, 7, 7, 7}, 3));
    EXPECT_DOUBLE_EQ(row.margin, 0.0);
}

TEST_F(StudyCommonTest, NoComparisonRan_AddsNoRow) {
    // Too little history for the lookback on the only bar evaluated
    MarginReport report("test", dir_ / "margins.csv", dir_ / "results");
    const std::vector<Family> probed =
        withMarginProbes({meanReversionFamily("MeanReversion")}, &report);
    const std::unique_ptr<Strategy> strategy = probed[0].make({5, 1.0, 0.0});
    runStrategy(*strategy, makeWindow({10, 11}, 1));
    EXPECT_TRUE(report.rows().empty());
}

TEST_F(StudyCommonTest, Probe_LeavesResultsAndNameUnchanged) {
    const WindowData data = makeWindow({10, 10, 10, 7, 8, 12, 13, 9, 7, 6, 11, 14, 15, 10}, 3);
    const Family family = meanReversionFamily("MeanReversion");
    const Params params = {3, 1.0, 0.0};

    const std::unique_ptr<Strategy> plain = family.make(params);
    const BacktestResult expected = runStrategy(*plain, data);

    MarginReport report("test", dir_ / "margins.csv", dir_ / "results");
    const std::vector<Family> probed = withMarginProbes({family}, &report);
    const std::unique_ptr<Strategy> strategy = probed[0].make(params);
    const BacktestResult actual = runStrategy(*strategy, data);

    EXPECT_GT(expected.totalTrades, 0);
    EXPECT_EQ(actual.equityCurve, expected.equityCurve);
    EXPECT_EQ(actual.totalTrades, expected.totalTrades);
    EXPECT_EQ(strategy->name(), plain->name());
}

TEST_F(StudyCommonTest, Probe_ReusedAcrossRuns_ReportsEachRunSeparately) {
    MarginReport report("test", dir_ / "margins.csv", dir_ / "results");
    const std::vector<Family> probed = withMarginProbes({crossoverFamily()}, &report);
    const std::unique_ptr<Strategy> strategy = probed[0].make({2, 3});
    runStrategy(*strategy, makeWindow({10, 11, 12, 15}, 3));
    runStrategy(*strategy, makeWindow({7, 7, 7, 7}, 3));
    ASSERT_EQ(report.rows().size(), 2U);
    EXPECT_NEAR(report.rows()[0].margin, 5.0 / 76.0, 1e-10);
    EXPECT_DOUBLE_EQ(report.rows()[1].margin, 0.0);
}

TEST_F(StudyCommonTest, WithoutReport_FamiliesAreUnchanged) {
    const std::vector<Family> families = withMarginProbes({crossoverFamily()}, nullptr);
    const std::unique_ptr<Strategy> strategy = families[0].make({2, 3});
    EXPECT_NE(dynamic_cast<MovingAverageCrossoverStrategy*>(strategy.get()), nullptr);
}

TEST_F(StudyCommonTest, UnknownFamily_Throws) {
    MarginReport report("test", dir_ / "margins.csv", dir_ / "results");
    Family family = crossoverFamily();
    family.name = "SomethingNew";
    const std::vector<Family> probed = withMarginProbes({family}, &report);
    EXPECT_THROW(probed[0].make({2, 3}), std::runtime_error);
}

TEST_F(StudyCommonTest, Report_InsideResultsFolder_Refused) {
    const fs::path results = dir_ / "results";
    EXPECT_THROW(MarginReport("test", results / "margins.csv", results), std::runtime_error);
    EXPECT_THROW(MarginReport("test", results / "deep" / "margins.csv", results),
                 std::runtime_error);
    EXPECT_THROW(MarginReport("test", results, results), std::runtime_error);
    EXPECT_THROW(MarginReport("test", dir_ / "other" / ".." / "results" / "margins.csv", results),
                 std::runtime_error);
}

TEST_F(StudyCommonTest, Report_InsideResultsThroughSymlink_Refused) {
    const fs::path results = dir_ / "results";
    fs::create_directories(results);
    fs::create_directory_symlink(results, dir_ / "link");
    EXPECT_THROW(MarginReport("test", dir_ / "link" / "margins.csv", results), std::runtime_error);
}

TEST_F(StudyCommonTest, Report_InResultsFolderOfCurrentDirectory_Refused) {
    const fs::path original = fs::current_path();
    fs::current_path(dir_);
    EXPECT_THROW(MarginReport("test", "results/margins.csv", dir_ / "elsewhere"),
                 std::runtime_error);
    fs::current_path(original);
}

TEST_F(StudyCommonTest, Report_SiblingOfResultsFolder_Allowed) {
    const fs::path results = dir_ / "results";
    EXPECT_NO_THROW(MarginReport("test", dir_ / "results_margins.csv", results));
    EXPECT_NO_THROW(MarginReport("test", dir_ / "other.csv", results));
}

TEST_F(StudyCommonTest, Report_ExistingFileThatIsNotAReport_RefusedAndLeftAlone) {
    const fs::path path = dir_ / "notes.csv";
    {
        std::ofstream out(path);
        out << "something,else\n";
    }
    EXPECT_THROW(MarginReport("test", path, dir_ / "results"), std::runtime_error);
    EXPECT_EQ(linesOf(path), std::vector<std::string>{"something,else"});
}

TEST_F(StudyCommonTest, Report_UnwritablePath_Throws) {
    EXPECT_THROW(MarginReport("test", dir_ / "missing" / "margins.csv", dir_ / "results"),
                 std::runtime_error);
}

TEST_F(StudyCommonTest, Report_Write_SortsSmallestFirst) {
    const fs::path path = dir_ / "margins.csv";
    MarginReport report("prog", path, dir_ / "results");
    report.add({"prog", "w", "Family", "AAA", "a=1", 0.5, "2023-01-01", "entry_z"});
    report.add({"prog", "w", "Family", "BBB", "a=2", 0.25, "2023-01-02", "exit_z"});
    report.write();
    const std::vector<std::string> expected = {
        "program,window,family,symbol,params,smallest_margin,date,comparison",
        "prog,w,Family,BBB,a=2,0.25,2023-01-02,exit_z",
        "prog,w,Family,AAA,a=1,0.5,2023-01-01,entry_z",
    };
    EXPECT_EQ(linesOf(path), expected);
}

TEST_F(StudyCommonTest, Report_Write_KeepsRowsAlreadyInTheFile) {
    const fs::path path = dir_ / "margins.csv";
    MarginReport first("one", path, dir_ / "results");
    first.add({"one", "w", "Family", "AAA", "", 0.5, "2023-01-01", "entry_z"});
    first.write();
    MarginReport second("two", path, dir_ / "results");
    second.add({"two", "w", "Family", "BBB", "a=2", 0.25, "2023-01-02", "exit_z"});
    second.write();
    const std::vector<std::string> expected = {
        "program,window,family,symbol,params,smallest_margin,date,comparison",
        "two,w,Family,BBB,a=2,0.25,2023-01-02,exit_z",
        "one,w,Family,AAA,,0.5,2023-01-01,entry_z",
    };
    EXPECT_EQ(linesOf(path), expected);
}

TEST_F(StudyCommonTest, Report_Write_KeepsFullPrecision) {
    const fs::path path = dir_ / "margins.csv";
    const double margin = 1.0 / 3.0;
    MarginReport report("prog", path, dir_ / "results");
    report.add({"prog", "w", "Family", "AAA", "", margin, "2023-01-01", "entry_z"});
    report.write();
    MarginReport again("prog", path, dir_ / "results");
    again.write();
    const std::string line = linesOf(path).at(1);
    std::istringstream fields(line);
    std::string field;
    for (int column = 0; column < 6; ++column) {
        std::getline(fields, field, ',');
    }
    EXPECT_EQ(std::stod(field), margin);
}

TEST_F(StudyCommonTest, Report_MalformedRowInFile_Throws) {
    const fs::path path = dir_ / "margins.csv";
    {
        std::ofstream out(path);
        out << "program,window,family,symbol,params,smallest_margin,date,comparison\nbroken\n";
    }
    EXPECT_THROW(MarginReport("test", path, dir_ / "results"), std::runtime_error);
}

TEST_F(StudyCommonTest, Environment_UnsetOrEmpty_NoReport) {
    unsetenv("BARCOLA_MARGIN_REPORT");
    EXPECT_FALSE(marginReportFromEnvironment("test", dir_ / "results").has_value());
    setenv("BARCOLA_MARGIN_REPORT", "", 1);
    EXPECT_FALSE(marginReportFromEnvironment("test", dir_ / "results").has_value());
    unsetenv("BARCOLA_MARGIN_REPORT");
}

TEST_F(StudyCommonTest, Environment_Set_ReportsThere) {
    const fs::path path = dir_ / "margins.csv";
    setenv("BARCOLA_MARGIN_REPORT", path.c_str(), 1);
    const auto report = marginReportFromEnvironment("test", dir_ / "results");
    unsetenv("BARCOLA_MARGIN_REPORT");
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->program(), "test");
    report->write();
    EXPECT_EQ(linesOf(path).size(), 1U);
}

TEST_F(StudyCommonTest, SliceWindow_RecordsTheWindowName) {
    const WindowData all = makeWindow(std::vector<double>(300, 10.0), 0);
    const Window window{"named", "Named", "2023-09-30", "2023-10-20"};
    EXPECT_EQ(sliceWindow(all.bars, window).window, "named");
}
