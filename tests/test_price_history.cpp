#include <gtest/gtest.h>
#include "core/price_history.h"

using namespace barcola;

static std::string dataDir() {
    return std::string(BARCOLA_TEST_DATA_DIR);
}

TEST(PriceHistoryTest, LoadFromCsv_CorrectCount) {
    PriceHistory history("AAPL");
    history.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");
    EXPECT_EQ(history.dataPointsCount(), 30u);
}

TEST(PriceHistoryTest, LoadFromCsv_FirstRow) {
    PriceHistory history("AAPL");
    history.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");
    auto first = history.getDataPoint(static_cast<size_t>(0));
    EXPECT_DOUBLE_EQ(first.getOpening(), 130.28);
    EXPECT_DOUBLE_EQ(first.getClosing(), 125.07);
    EXPECT_EQ(first.getDateString(), "2023-01-03");
}

TEST(PriceHistoryTest, GetDataPointByDate) {
    PriceHistory history("AAPL");
    history.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");
    auto dp = history.getDataPoint(std::string("2023-01-06"));
    EXPECT_DOUBLE_EQ(dp.getClosing(), 129.62);
}

TEST(PriceHistoryTest, GetDataPoint_OutOfRange_Throws) {
    PriceHistory history("AAPL");
    history.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");
    EXPECT_THROW((void)history.getDataPoint(static_cast<size_t>(999)), std::invalid_argument);
}

TEST(PriceHistoryTest, GetDataPoint_InvalidDate_Throws) {
    PriceHistory history("AAPL");
    history.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");
    EXPECT_THROW((void)history.getDataPoint(std::string("2099-01-01")), std::invalid_argument);
}

TEST(PriceHistoryTest, ClearDataPoints) {
    PriceHistory history("AAPL");
    history.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");
    EXPECT_GT(history.dataPointsCount(), 0u);
    history.clearDataPoints();
    EXPECT_EQ(history.dataPointsCount(), 0u);
}

TEST(PriceHistoryTest, EmptyCsv_ZeroDataPoints) {
    PriceHistory history("EMPTY");
    history.loadFromCsv(dataDir() + "/empty.csv");
    EXPECT_EQ(history.dataPointsCount(), 0u);
}

TEST(PriceHistoryTest, SinglePointCsv) {
    PriceHistory history("SINGLE");
    history.loadFromCsv(dataDir() + "/single_point.csv");
    EXPECT_EQ(history.dataPointsCount(), 1u);
    EXPECT_DOUBLE_EQ(history.getDataPoint(static_cast<size_t>(0)).getClosing(), 152.0);
}

TEST(PriceHistoryTest, GetAssetSymbol) {
    PriceHistory history("SPY");
    EXPECT_EQ(history.getAssetSymbol(), "SPY");
}

TEST(PriceHistoryTest, GetDataPoints_ReturnsConstRef) {
    PriceHistory history("AAPL");
    history.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");
    const auto& points = history.getDataPoints();
    EXPECT_EQ(points.size(), 30u);
}

TEST(PriceHistoryTest, GetClosingPrices) {
    PriceHistory history("UP");
    history.loadFromCsv(dataDir() + "/monotonic_up.csv");
    const auto closes = history.getClosingPrices();
    ASSERT_EQ(closes.size(), 16u);
    EXPECT_DOUBLE_EQ(closes.front(), 10.0);
    EXPECT_DOUBLE_EQ(closes.back(), 25.0);
}

TEST(PriceHistoryTest, AddDataPoint_AppendsInDateOrder) {
    PriceHistory history("TEST");
    history.addDataPoint(PricePoint("2023-01-03", 10.0));
    history.addDataPoint(PricePoint("2023-01-04", 11.0));
    EXPECT_EQ(history.dataPointsCount(), 2u);
    EXPECT_DOUBLE_EQ(history.getDataPoint(std::string("2023-01-04")).getClosing(), 11.0);
}

TEST(PriceHistoryTest, AddDataPoint_OutOfOrderOrDuplicate_Throws) {
    PriceHistory history("TEST");
    history.addDataPoint(PricePoint("2023-01-04", 10.0));
    EXPECT_THROW(history.addDataPoint(PricePoint("2023-01-03", 9.0)), std::invalid_argument);
    EXPECT_THROW(history.addDataPoint(PricePoint("2023-01-04", 9.0)), std::invalid_argument);
}
