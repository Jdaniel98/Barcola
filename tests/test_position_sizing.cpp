#include <gtest/gtest.h>
#include "risk/position_sizing.h"

using namespace barcola;

TEST(PositionSizingTest, KnownValues) {
    // risk=2.5%, stopLoss=2%
    // positionSize = (100000 * 0.025) / (50 * 0.02) = 2500 / 1.0 = 2500
    PositionSizing ps(0.025, 0.02);
    double result = ps.calculatePositionSize(100000.0, 50.0);
    EXPECT_DOUBLE_EQ(result, 2500.0);
}

TEST(PositionSizingTest, WithMaxLoss) {
    // maxLoss=1000, entryPrice=50, stopLoss=2%
    // positionSize = 1000 / (50 * 0.02) = 1000 / 1.0 = 1000
    PositionSizing ps(0.025, 0.02);
    double result = ps.calculatePositionSizeWithMaxLoss(100000.0, 50.0, 1000.0);
    EXPECT_DOUBLE_EQ(result, 1000.0);
}

TEST(PositionSizingTest, SettersAndGetters) {
    PositionSizing ps(0.01, 0.05);
    EXPECT_DOUBLE_EQ(ps.getRiskPercentage(), 0.01);
    EXPECT_DOUBLE_EQ(ps.getStopLossPercentage(), 0.05);

    ps.setRiskPercentage(0.03);
    ps.setStopLossPercentage(0.10);
    EXPECT_DOUBLE_EQ(ps.getRiskPercentage(), 0.03);
    EXPECT_DOUBLE_EQ(ps.getStopLossPercentage(), 0.10);
}
