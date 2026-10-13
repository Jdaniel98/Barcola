#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
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

// Writes `contents` to a uniquely named temp file and removes it when it goes out of scope
class TempCsv {
public:
    TempCsv(const std::string& name, const std::string& contents)
        : path_(std::filesystem::temp_directory_path() / ("barcola_" + name + ".csv")) {
        std::ofstream file(path_);
        file << contents;
    }
    ~TempCsv() { std::filesystem::remove(path_); }
    [[nodiscard]] std::string path() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

TEST(CsvIOTest, LoadTwice_ReplacesData) {
    // Regression: each load appended, so loading twice duplicated every bar
    PriceHistory history("AAPL");
    history.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");
    history.loadFromCsv(dataDir() + "/AAPL_2023_sample.csv");
    EXPECT_EQ(history.dataPointsCount(), 30u);
}

TEST(CsvIOTest, UnsortedRows_AreSorted) {
    TempCsv csv("unsorted", "Date,Open,High,Low,Close\n"
                            "2023-01-05,3,3,3,3\n"
                            "2023-01-03,1,1,1,1\n"
                            "2023-01-04,2,2,2,2\n");
    PriceHistory history("TEST");
    history.loadFromCsv(csv.path());
    ASSERT_EQ(history.dataPointsCount(), 3u);
    EXPECT_EQ(history.getDataPoint(static_cast<size_t>(0)).getDateString(), "2023-01-03");
    EXPECT_EQ(history.getDataPoint(static_cast<size_t>(2)).getDateString(), "2023-01-05");
}

TEST(CsvIOTest, DuplicateDate_Throws) {
    TempCsv csv("duplicate", "Date,Open,High,Low,Close\n"
                             "2023-01-03,1,1,1,1\n"
                             "2023-01-03,2,2,2,2\n");
    PriceHistory history("TEST");
    EXPECT_THROW(history.loadFromCsv(csv.path()), std::runtime_error);
}

TEST(CsvIOTest, MalformedNumber_ErrorNamesLine) {
    TempCsv csv("malformed", "Date,Open,High,Low,Close\n"
                             "2023-01-03,1,1,1,1\n"
                             "2023-01-04,1,abc,1,1\n");
    PriceHistory history("TEST");
    try {
        history.loadFromCsv(csv.path());
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find(":3:"), std::string::npos) << message;
        EXPECT_NE(message.find("abc"), std::string::npos) << message;
    }
}

TEST(CsvIOTest, MissingColumn_Throws) {
    TempCsv csv("missing_column", "Date,Open,High,Low\n2023-01-03,1,1,1\n");
    PriceHistory history("TEST");
    EXPECT_THROW(history.loadFromCsv(csv.path()), std::runtime_error);
}

TEST(CsvIOTest, NullRows_Skipped) {
    TempCsv csv("nulls", "Date,Open,High,Low,Close\n"
                         "2023-01-03,1,1,1,1\n"
                         "2023-01-04,null,null,null,null\n"
                         "2023-01-05,3,3,3,3\n");
    PriceHistory history("TEST");
    history.loadFromCsv(csv.path());
    EXPECT_EQ(history.dataPointsCount(), 2u);
}

TEST(CsvIOTest, AdjClose_ScalesAllPrices) {
    // Yahoo layout. Adj Close / Close = 0.5, so every price is halved.
    TempCsv csv("adjusted", "Date,Open,High,Low,Close,Adj Close,Volume\n"
                            "2023-01-03,100,110,90,104,52,1000\n");
    PriceHistory history("TEST");
    history.loadFromCsv(csv.path());
    ASSERT_EQ(history.dataPointsCount(), 1u);
    const auto bar = history.getDataPoint(static_cast<size_t>(0));
    EXPECT_DOUBLE_EQ(bar.getOpening(), 50.0);
    EXPECT_DOUBLE_EQ(bar.getHighest(), 55.0);
    EXPECT_DOUBLE_EQ(bar.getLowest(), 45.0);
    EXPECT_DOUBLE_EQ(bar.getClosing(), 52.0);
}

TEST(CsvIOTest, ColumnsFoundByHeaderName) {
    TempCsv csv("reordered", "Close,Date,Low,High,Open\n104,2023-01-03,90,110,100\n");
    PriceHistory history("TEST");
    history.loadFromCsv(csv.path());
    const auto bar = history.getDataPoint(static_cast<size_t>(0));
    EXPECT_DOUBLE_EQ(bar.getOpening(), 100.0);
    EXPECT_DOUBLE_EQ(bar.getClosing(), 104.0);
}

TEST(CsvIOTest, WindowsLineEndings) {
    TempCsv csv("crlf", "Date,Open,High,Low,Close\r\n2023-01-03,1,2,0.5,1.5\r\n");
    PriceHistory history("TEST");
    history.loadFromCsv(csv.path());
    ASSERT_EQ(history.dataPointsCount(), 1u);
    EXPECT_DOUBLE_EQ(history.getDataPoint(static_cast<size_t>(0)).getClosing(), 1.5);
}
