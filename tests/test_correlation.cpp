#include <gtest/gtest.h>

#include "analysis/correlation.h"

using namespace barcola;

TEST(CorrelationTest, PerfectPositive) {
    std::vector<double> a = {1, 2, 3, 4, 5};
    std::vector<double> b = {2, 4, 6, 8, 10};
    EXPECT_NEAR(calculateCorrelation(a, b), 1.0, 1e-10);
}

TEST(CorrelationTest, PerfectNegative) {
    std::vector<double> a = {1, 2, 3, 4, 5};
    std::vector<double> b = {10, 8, 6, 4, 2};
    EXPECT_NEAR(calculateCorrelation(a, b), -1.0, 1e-10);
}

TEST(CorrelationTest, ZeroVariance_ReturnsZero) {
    // Bug 2 regression test: constant series has zero variance
    std::vector<double> a = {100, 100, 100, 100, 100};
    std::vector<double> b = {1, 2, 3, 4, 5};
    EXPECT_DOUBLE_EQ(calculateCorrelation(a, b), 0.0);
}

TEST(CorrelationTest, BothConstant_ReturnsZero) {
    std::vector<double> a = {5, 5, 5};
    std::vector<double> b = {10, 10, 10};
    EXPECT_DOUBLE_EQ(calculateCorrelation(a, b), 0.0);
}

TEST(CorrelationTest, SinglePoint_ReturnsZero) {
    std::vector<double> a = {42};
    std::vector<double> b = {99};
    EXPECT_DOUBLE_EQ(calculateCorrelation(a, b), 0.0);
}

TEST(CorrelationTest, CorrelationAnalysisClass) {
    PriceHistory histA("AAPL");
    PriceHistory histB("MSFT");
    histA.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");
    histB.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/MSFT_2023_sample.csv");

    CorrelationAnalysis analysis(histA, histB);
    // Correlation of daily log returns, computed independently. (The price levels correlate
    // at 0.997 because both stocks rose over the sample.)
    EXPECT_NEAR(analysis.calculateCorrelation(), 0.4715678594, 1e-9);
}

// History with one bar per listed date
static PriceHistory makeHistory(const std::vector<std::string>& dates,
                                const std::vector<double>& closes) {
    PriceHistory history("TEST");
    for (size_t i = 0; i < dates.size(); ++i) {
        history.addDataPoint(PricePoint(dates[i], closes[i]));
    }
    return history;
}

TEST(CorrelationTest, TrendingAssets_CorrelateOnReturnsNotLevels) {
    // Regression: both series trend up, so their price levels correlate strongly, but their
    // daily moves are opposite (A: +3, -1, ... and B: -1, +3, ...).
    // Levels correlate at about 0.61, returns at about -1.
    const std::vector<std::string> dates = {"2023-01-02", "2023-01-03", "2023-01-04", "2023-01-05",
                                            "2023-01-06", "2023-01-09", "2023-01-10"};
    const PriceHistory a = makeHistory(dates, {100, 103, 102, 105, 104, 107, 106});
    const PriceHistory b = makeHistory(dates, {100, 99, 102, 101, 104, 103, 106});

    EXPECT_GT(calculateCorrelation(a.getClosingPrices(), b.getClosingPrices()), 0.5);
    EXPECT_LT(CorrelationAnalysis(a, b).calculateCorrelation(), -0.9);
}

TEST(CorrelationTest, MissingDay_AlignedByDate) {
    // Regression: rows were paired by position, so one missing day shifted every later pair.
    // B is A without 2023-01-04, so on their common dates the two are identical.
    const PriceHistory a = makeHistory(
        {"2023-01-02", "2023-01-03", "2023-01-04", "2023-01-05", "2023-01-06", "2023-01-09"},
        {100, 103, 99, 104, 101, 106});
    const PriceHistory b =
        makeHistory({"2023-01-02", "2023-01-03", "2023-01-05", "2023-01-06", "2023-01-09"},
                    {100, 103, 104, 101, 106});

    EXPECT_NEAR(CorrelationAnalysis(a, b).calculateCorrelation(), 1.0, 1e-12);
}

TEST(CorrelationTest, NoCommonDates_ReturnsZero) {
    const PriceHistory a = makeHistory({"2023-01-02", "2023-01-03", "2023-01-04"}, {1, 2, 3});
    const PriceHistory b = makeHistory({"2023-02-02", "2023-02-03", "2023-02-06"}, {1, 2, 3});
    EXPECT_DOUBLE_EQ(CorrelationAnalysis(a, b).calculateCorrelation(), 0.0);
}
