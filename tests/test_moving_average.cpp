#include <gtest/gtest.h>
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

TEST(MovingAverageTest, PriceHistorySMA_KnownValues) {
    // SMA(3) for prices [10, 20, 30, 40, 50]:
    // Index 2: (10+20+30)/3 = 20.0
    // Index 3: (20+30+40)/3 = 30.0
    // Index 4: (30+40+50)/3 = 40.0
    PriceHistory history("TEST");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    auto [sma, _] = history.calculateSMA(3);
    // First valid SMA is at index 2 (3rd element)
    // Prices: 10, 11, 12, ... SMA(3) at idx 2 = (10+11+12)/3 = 11.0
    EXPECT_NEAR(sma[2], 11.0, 1e-10);
    // SMA(3) at idx 3 = (11+12+13)/3 = 12.0
    EXPECT_NEAR(sma[3], 12.0, 1e-10);
}
