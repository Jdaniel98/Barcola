#include <gtest/gtest.h>
#include <filesystem>
#include "core/price_history.h"

using namespace barcola;

static std::string dataDir() {
    return std::string(BARCOLA_TEST_DATA_DIR);
}

TEST(CsvIOTest, LoadEmptyCsv_NoCrash) {
    PriceHistory history("EMPTY");
    EXPECT_NO_THROW(history.loadFromCsv(dataDir() + "/empty.csv"));
    EXPECT_EQ(history.dataPointsCount(), 0u);
}

TEST(CsvIOTest, LoadSinglePointCsv) {
    PriceHistory history("SINGLE");
    EXPECT_NO_THROW(history.loadFromCsv(dataDir() + "/single_point.csv"));
    EXPECT_EQ(history.dataPointsCount(), 1u);
}

TEST(CsvIOTest, LoadConstantPriceCsv) {
    PriceHistory history("CONST");
    EXPECT_NO_THROW(history.loadFromCsv(dataDir() + "/constant_price.csv"));
    EXPECT_EQ(history.dataPointsCount(), 10u);
}

TEST(CsvIOTest, LoadMonotonicUpCsv) {
    PriceHistory history("UP");
    EXPECT_NO_THROW(history.loadFromCsv(dataDir() + "/monotonic_up.csv"));
    EXPECT_EQ(history.dataPointsCount(), 16u);
}

TEST(CsvIOTest, LoadMonotonicDownCsv) {
    PriceHistory history("DOWN");
    EXPECT_NO_THROW(history.loadFromCsv(dataDir() + "/monotonic_down.csv"));
    EXPECT_EQ(history.dataPointsCount(), 16u);
}

TEST(CsvIOTest, SaveAndReload_RoundTrip) {
    PriceHistory original("AAPL");
    original.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");

    const std::string tempPath = dataDir() + "/test_roundtrip_output.csv";
    original.saveToCsv(tempPath);

    PriceHistory reloaded("AAPL");
    reloaded.loadFromCsv(tempPath);

    EXPECT_EQ(original.dataPointsCount(), reloaded.dataPointsCount());

    for (size_t i = 0; i < original.dataPointsCount(); ++i) {
        auto orig = original.getDataPoint(i);
        auto reload = reloaded.getDataPoint(i);
        EXPECT_EQ(orig.getDateString(), reload.getDateString());
        EXPECT_NEAR(orig.getClosing(), reload.getClosing(), 0.01);
        EXPECT_NEAR(orig.getOpening(), reload.getOpening(), 0.01);
    }

    // Cleanup
    std::filesystem::remove(tempPath);
}

TEST(CsvIOTest, NonExistentFile_Throws) {
    PriceHistory history("NONE");
    EXPECT_THROW(history.loadFromCsv("/nonexistent/path.csv"), std::runtime_error);
}
