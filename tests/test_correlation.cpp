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
    double corr = analysis.calculateCorrelation();
    // AAPL and MSFT should be positively correlated
    EXPECT_GT(corr, 0.0);
    EXPECT_LE(corr, 1.0);
}
