#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include "core/time_utils.h"
#include "simulation/monte_carlo.h"

using namespace barcola;

static constexpr time_t kSecondsPerDay = 86400;

// A history with one bar per calendar day starting 2023-01-01
static PriceHistory makeHistory(const std::vector<double>& closes) {
    PriceHistory history("SYNTH");
    const time_t start = convertDateToEpoch("2023-01-01");
    for (size_t i = 0; i < closes.size(); ++i) {
        history.addDataPoint(PricePoint(start + static_cast<time_t>(i) * kSecondsPerDay, closes[i]));
    }
    return history;
}

// Trending, wavy series long enough for the 50-day average
static std::vector<double> wavyCloses() {
    std::vector<double> closes;
    for (int i = 0; i < 200; ++i) {
        closes.push_back(100.0 + 0.1 * i + 10.0 * std::sin(i / 7.0) + 3.0 * std::sin(i * 1.3));
    }
    return closes;
}

TEST(MonteCarloTest, InsufficientData_EmptyResult) {
    PriceHistory history("UP");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    // Only 16 data points, need 51+ for SMA(50)
    const auto result = performMonteCarloSimulation(history, 10, 100000.0);
    EXPECT_TRUE(result.profits.empty());
    EXPECT_DOUBLE_EQ(result.meanProfit, 0.0);
}

TEST(MonteCarloTest, EmptyData_EmptyResult) {
    PriceHistory history("EMPTY");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/empty.csv");

    const auto result = performMonteCarloSimulation(history, 10, 100000.0);
    EXPECT_TRUE(result.profits.empty());
}

TEST(MonteCarloTest, InvalidArguments_Throw) {
    const PriceHistory history = makeHistory(wavyCloses());
    EXPECT_THROW((void)performMonteCarloSimulation(history, 0, 100000.0), std::invalid_argument);
    EXPECT_THROW((void)performMonteCarloSimulation(history, 10, 0.0), std::invalid_argument);
}

TEST(MonteCarloTest, SimulationsDiffer) {
    // Regression: every simulation replayed the same prices, so all results were identical
    const PriceHistory history = makeHistory(wavyCloses());
    const auto result = performMonteCarloSimulation(history, 200, 100000.0);

    ASSERT_EQ(result.profits.size(), 200u);
    EXPECT_LT(result.percentile5, result.percentile95);
    EXPECT_LE(result.profits.front(), result.percentile5);
    EXPECT_GE(result.profits.back(), result.percentile95);
    EXPECT_TRUE(std::is_sorted(result.profits.begin(), result.profits.end()));
}

TEST(MonteCarloTest, SameSeed_SameResult) {
    const PriceHistory history = makeHistory(wavyCloses());
    const auto first = performMonteCarloSimulation(history, 50, 100000.0, 7);
    const auto second = performMonteCarloSimulation(history, 50, 100000.0, 7);
    EXPECT_EQ(first.profits, second.profits);
}

TEST(MonteCarloTest, DifferentSeed_DifferentResult) {
    const PriceHistory history = makeHistory(wavyCloses());
    const auto first = performMonteCarloSimulation(history, 50, 100000.0, 7);
    const auto second = performMonteCarloSimulation(history, 50, 100000.0, 8);
    EXPECT_NE(first.profits, second.profits);
}

TEST(SmaCrossoverTest, FallThenRise_BuysOnGoldenCross) {
    // Regression: buy and sell were swapped. Prices fall from 100 to 70, then rise to 130.
    // The short average crosses above the long one during the rise, so the rule buys and
    // ends with a profit. The old rule never bought here.
    std::vector<double> closes;
    for (int i = 0; i < 60; ++i) closes.push_back(100.0 - 0.5 * i);
    for (int i = 1; i <= 60; ++i) closes.push_back(70.0 + i);

    EXPECT_GT(runSmaCrossover(closes, 100000.0), 0.0);
}

TEST(SmaCrossoverTest, RiseThenFall_NeverBuys) {
    // The only crossover is a death cross, which sells, and there is nothing to sell.
    // The old rule bought on it and lost money as prices fell.
    std::vector<double> closes;
    for (int i = 0; i < 60; ++i) closes.push_back(70.0 + 0.5 * i);
    for (int i = 1; i <= 60; ++i) closes.push_back(100.0 - i);

    EXPECT_DOUBLE_EQ(runSmaCrossover(closes, 100000.0), 0.0);
}

TEST(SmaCrossoverTest, TooShort_ReturnsZero) {
    std::vector<double> closes(MONTE_CARLO_MIN_BARS - 1, 100.0);
    EXPECT_DOUBLE_EQ(runSmaCrossover(closes, 100000.0), 0.0);
}
