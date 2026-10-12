#include <gtest/gtest.h>
#include "risk/position_sizing.h"

using namespace barcola;

TEST(PositionSizingTest, KnownValues) {
    // risk=1%, stopLoss=2%
    // positionSize = (100000 * 0.01) / (50 * 0.02) = 1000 / 1.0 = 1000 shares ($50,000)
    PositionSizing ps(0.01, 0.02);
    EXPECT_DOUBLE_EQ(ps.calculatePositionSize(100000.0, 50.0), 1000.0);
}

TEST(PositionSizingTest, CappedAtPortfolioValueByDefault) {
    // Regression: risk=2.5%, stop=2% asks for 2500 shares ($125,000), more than the
    // $100,000 portfolio. Without leverage the cap is 100000 / 50 = 2000 shares.
    PositionSizing ps(0.025, 0.02);
    EXPECT_DOUBLE_EQ(ps.calculatePositionSize(100000.0, 50.0), 2000.0);
}

TEST(PositionSizingTest, LeverageAllowedWhenRequested) {
    PositionSizing ps(0.025, 0.02, 2.0);
    EXPECT_DOUBLE_EQ(ps.calculatePositionSize(100000.0, 50.0), 2500.0);
}

TEST(PositionSizingTest, WithMaxLoss) {
    // maxLoss=1000, entryPrice=50, stopLoss=2%
    // positionSize = 1000 / (50 * 0.02) = 1000 / 1.0 = 1000
    PositionSizing ps(0.025, 0.02);
    double result = ps.calculatePositionSizeWithMaxLoss(100000.0, 50.0, 1000.0);
    EXPECT_DOUBLE_EQ(result, 1000.0);
}

TEST(PositionSizingTest, WithMaxLoss_UsesPortfolioForCap) {
    // Regression: portfolioSize was ignored. 5000 / (50 * 0.02) = 5000 shares ($250,000)
    // is capped to 100000 / 50 = 2000 shares.
    PositionSizing ps(0.025, 0.02);
    EXPECT_DOUBLE_EQ(ps.calculatePositionSizeWithMaxLoss(100000.0, 50.0, 5000.0), 2000.0);
}

TEST(PositionSizingTest, SettersAndGetters) {
    PositionSizing ps(0.01, 0.05);
    EXPECT_DOUBLE_EQ(ps.getRiskPercentage(), 0.01);
    EXPECT_DOUBLE_EQ(ps.getStopLossPercentage(), 0.05);
    EXPECT_DOUBLE_EQ(ps.getMaxLeverage(), 1.0);

    ps.setRiskPercentage(0.03);
    ps.setStopLossPercentage(0.10);
    EXPECT_DOUBLE_EQ(ps.getRiskPercentage(), 0.03);
    EXPECT_DOUBLE_EQ(ps.getStopLossPercentage(), 0.10);
}

TEST(PositionSizingTest, InvalidInputs_Throw) {
    EXPECT_THROW(PositionSizing(0.0, 0.02), std::invalid_argument);
    EXPECT_THROW(PositionSizing(0.01, 0.0), std::invalid_argument);
    EXPECT_THROW(PositionSizing(0.01, 1.0), std::invalid_argument);
    EXPECT_THROW(PositionSizing(0.01, 0.02, 0.0), std::invalid_argument);

    PositionSizing ps(0.01, 0.02);
    EXPECT_THROW((void)ps.calculatePositionSize(-1000.0, 50.0), std::invalid_argument);
    EXPECT_THROW((void)ps.calculatePositionSize(100000.0, 0.0), std::invalid_argument);
    EXPECT_THROW((void)ps.calculatePositionSizeWithMaxLoss(100000.0, 50.0, -1.0),
                 std::invalid_argument);
    EXPECT_THROW(ps.setStopLossPercentage(-0.1), std::invalid_argument);
}
