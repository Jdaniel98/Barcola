#include <gtest/gtest.h>

#include <cmath>

#include "indicators/moving_average.h"

using namespace barcola;

TEST(MovingAverageTest, SMA_KnownValues) {
    MovingAverage ma(3);
    ma.addData(10.0);
    ma.addData(20.0);
    ma.addData(30.0);
    EXPECT_DOUBLE_EQ(ma.getSMA(), 20.0);
}

TEST(MovingAverageTest, SMA_WindowOverflow) {
    MovingAverage ma(3);
    ma.addData(10.0);
    ma.addData(20.0);
    ma.addData(30.0);
    ma.addData(40.0);
    // Window: [20, 30, 40]
    EXPECT_DOUBLE_EQ(ma.getSMA(), 30.0);
}

TEST(MovingAverageTest, SMA_Empty) {
    MovingAverage ma(3);
    EXPECT_DOUBLE_EQ(ma.getSMA(), 0.0);
}

TEST(MovingAverageTest, EMA_SinglePoint) {
    MovingAverage ma(3);
    ma.addData(42.0);
    EXPECT_DOUBLE_EQ(ma.getEMA(0.5), 42.0);
}

TEST(MovingAverageTest, EMA_Empty) {
    MovingAverage ma(3);
    EXPECT_DOUBLE_EQ(ma.getEMA(0.5), 0.0);
}

TEST(MovingAverageTest, EMA_KnownValues) {
    // Forward EMA with smoothing=0.5: start at first value, blend forward
    MovingAverage ma(5);
    ma.addData(10.0);
    ma.addData(20.0);
    ma.addData(30.0);
    double ema = ma.getEMA(0.5);
    // ema[0]=10, ema[1]=0.5*20+0.5*10=15, ema[2]=0.5*30+0.5*15=22.5
    EXPECT_DOUBLE_EQ(ema, 22.5);
}

TEST(MovingAverageTest, EMA_InvalidSmoothing_Throws) {
    MovingAverage ma(3);
    ma.addData(1.0);
    EXPECT_THROW((void)ma.getEMA(0.0), std::invalid_argument);
    EXPECT_THROW((void)ma.getEMA(1.5), std::invalid_argument);
}

TEST(MovingAverageTest, ZeroWindow_Throws) {
    EXPECT_THROW(MovingAverage(0), std::invalid_argument);
    EXPECT_THROW((void)calculateSMA({1.0, 2.0}, 0), std::invalid_argument);
    // Regression: PriceHistory::calculateEMA(0, a) wrote past the end of its buffer
    EXPECT_THROW((void)calculateEMA({1.0, 2.0}, 0), std::invalid_argument);
}

TEST(MovingAverageTest, SeriesSMA_KnownValues) {
    PriceHistory history("TEST");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    const auto sma = calculateSMA(history.getClosingPrices(), 3);
    ASSERT_EQ(sma.size(), history.dataPointsCount());
    // Not enough data for the first two bars
    EXPECT_TRUE(std::isnan(sma[0]));
    EXPECT_TRUE(std::isnan(sma[1]));
    // Prices: 10, 11, 12, ... SMA(3) at idx 2 = (10+11+12)/3 = 11.0
    EXPECT_NEAR(sma[2], 11.0, 1e-10);
    // SMA(3) at idx 3 = (11+12+13)/3 = 12.0
    EXPECT_NEAR(sma[3], 12.0, 1e-10);
}

TEST(MovingAverageTest, SeriesSMA_ShorterThanWindow_AllNaN) {
    const auto sma = calculateSMA({1.0, 2.0}, 3);
    ASSERT_EQ(sma.size(), 2u);
    EXPECT_TRUE(std::isnan(sma[0]));
    EXPECT_TRUE(std::isnan(sma[1]));
}

TEST(MovingAverageTest, SeriesEMA_KnownValues) {
    // window 3 => alpha = 2 / 4 = 0.5, seeded with SMA(10, 20, 30) = 20
    // idx 3: 0.5 * 40 + 0.5 * 20 = 30; idx 4: 0.5 * 10 + 0.5 * 30 = 20
    const auto ema = calculateEMA({10.0, 20.0, 30.0, 40.0, 10.0}, 3);
    ASSERT_EQ(ema.size(), 5u);
    EXPECT_TRUE(std::isnan(ema[1]));
    EXPECT_DOUBLE_EQ(ema[2], 20.0);
    EXPECT_DOUBLE_EQ(ema[3], 30.0);
    EXPECT_DOUBLE_EQ(ema[4], 20.0);
}
