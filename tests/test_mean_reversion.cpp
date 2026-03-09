#include <gtest/gtest.h>
#include "analysis/mean_reversion.h"

using namespace barcola;

TEST(MeanReversionTest, ConstantPrices_ZeroDeviation) {
    PriceHistory history("CONST");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/constant_price.csv");

    MeanReversion mr(history, 5);
    mr.calculateMeanReversion();

    const auto& values = mr.getValues();
    EXPECT_GT(values.size(), 0u);
    for (double val : values) {
        EXPECT_NEAR(val, 0.0, 1e-10);
    }
}

TEST(MeanReversionTest, AverageDeviationOfConstant_IsZero) {
    PriceHistory history("CONST");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/constant_price.csv");

    MeanReversion mr(history, 3);
    mr.calculateMeanReversion();
    EXPECT_NEAR(mr.getAverageMeanReversion(), 0.0, 1e-10);
}

TEST(MeanReversionTest, InsufficientData_EmptyValues) {
    PriceHistory history("SINGLE");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/single_point.csv");

    MeanReversion mr(history, 5);
    mr.calculateMeanReversion();
    EXPECT_EQ(mr.getValues().size(), 0u);
}

TEST(MeanReversionTest, KnownValues_MonotonicUp) {
    PriceHistory history("UP");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    MeanReversion mr(history, 3);
    mr.calculateMeanReversion();

    // Prices: 10, 11, 12, 13, ...
    // Window=3, first value at idx 2: MA=(10+11+12)/3=11.0, deviation=12-11=1.0
    EXPECT_NEAR(mr.getMeanReversionValue(0), 1.0, 1e-10);
}

TEST(MeanReversionTest, GetValueOutOfRange_Throws) {
    PriceHistory history("CONST");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/constant_price.csv");

    MeanReversion mr(history, 3);
    mr.calculateMeanReversion();
    EXPECT_THROW(mr.getMeanReversionValue(999), std::out_of_range);
}
