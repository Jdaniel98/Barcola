#include <gtest/gtest.h>

#include <vector>

#include "core/time_utils.h"
#include "detail/window_stats.h"

using namespace barcola;
using namespace barcola::detail;

// One bar per day from 2023-01-01, with open == close
static std::vector<PricePoint> makePrices(const std::vector<double>& closes) {
    std::vector<PricePoint> points;
    const time_t start = convertDateToEpoch("2023-01-01");
    for (size_t i = 0; i < closes.size(); ++i) {
        points.emplace_back(start + static_cast<time_t>(i) * 86400, closes[i]);
    }
    return points;
}

TEST(WindowStatsTest, TrailingMean_WholeSeries) {
    const auto prices = makePrices({1, 2, 3, 4});
    EXPECT_DOUBLE_EQ(trailingMean(BarView(prices, prices.size()), 4), 2.5);
}

TEST(WindowStatsTest, TrailingMean_UsesOnlyTheLastBars) {
    const auto prices = makePrices({100, 7, 9});
    EXPECT_DOUBLE_EQ(trailingMean(BarView(prices, prices.size()), 2), 8.0);
}

TEST(WindowStatsTest, TrailingMean_StopsAtTheViewsLastBar) {
    const auto prices = makePrices({1, 3, 100});
    EXPECT_DOUBLE_EQ(trailingMean(BarView(prices, 2), 2), 2.0);
}

TEST(WindowStatsTest, ZScore_HandCalculated) {
    // Mean 5, squared differences sum to 32, population standard deviation 2, so (9 - 5) / 2
    const auto prices = makePrices({2, 4, 4, 4, 5, 5, 7, 9});
    EXPECT_DOUBLE_EQ(zScore(BarView(prices, prices.size()), 8), 2.0);
}

TEST(WindowStatsTest, ZScore_BelowTheMeanIsNegative) {
    // Mean 2, squared differences sum to 2, standard deviation sqrt(2/3)
    const auto prices = makePrices({3, 2, 1});
    EXPECT_NEAR(zScore(BarView(prices, prices.size()), 3), -1.0 / std::sqrt(2.0 / 3.0), 1e-10);
}

TEST(WindowStatsTest, ZScore_UsesOnlyTheLookback) {
    const auto prices = makePrices({1000, 2, 4});
    EXPECT_DOUBLE_EQ(zScore(BarView(prices, prices.size()), 2), 1.0);
}

TEST(WindowStatsTest, ZScore_FlatPrices_ReturnsZero) {
    const auto prices = makePrices({5, 5, 5});
    EXPECT_DOUBLE_EQ(zScore(BarView(prices, prices.size()), 3), 0.0);
}

TEST(WindowStatsTest, RelativeGap_HandCalculated) {
    EXPECT_NEAR(relativeGap(110.0, 100.0), 0.1, 1e-10);
    EXPECT_NEAR(relativeGap(90.0, 100.0), -0.1, 1e-10);
    EXPECT_DOUBLE_EQ(relativeGap(100.0, 100.0), 0.0);
}

TEST(WindowStatsTest, RelativeGap_ZeroBase_ReturnsZero) {
    EXPECT_DOUBLE_EQ(relativeGap(5.0, 0.0), 0.0);
}
