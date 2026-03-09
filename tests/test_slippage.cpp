#include <gtest/gtest.h>
#include <cmath>
#include "risk/slippage_model.h"

using namespace barcola;

TEST(SlippageTest, BasicCalculation_NoCrash) {
    PriceHistory history("AAPL");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");

    SlippageModel model(history, 0.01);

    // Use dates from the data
    time_t entry = history.getDataPoint(std::string("2023-01-03")).getDate();
    time_t exit = history.getDataPoint(std::string("2023-01-06")).getDate();

    // Should not throw
    double slippage = model.calculateSlippage(entry, 126.0, exit, 130.0, 10000.0);
    // Result is non-deterministic due to random volatility, just check it's finite
    EXPECT_FALSE(std::isnan(slippage));
    EXPECT_FALSE(std::isinf(slippage));
}
