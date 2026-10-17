#include <gtest/gtest.h>

#include <cmath>

#include "core/time_utils.h"
#include "risk/slippage_model.h"

using namespace barcola;

static PriceHistory loadAapl() {
    PriceHistory history("AAPL");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/AAPL_2023_sample.csv");
    return history;
}

static time_t dateOf(const PriceHistory& history, size_t index) {
    return history.getDataPoint(index).getDate();
}

TEST(SlippageTest, Deterministic) {
    // Regression: the old model drew random numbers, so every call gave a different answer
    const PriceHistory history = loadAapl();
    SlippageModel model(history, 5.0);
    const time_t date = dateOf(history, 25);
    EXPECT_DOUBLE_EQ(model.slippageFraction(date), model.slippageFraction(date));
}

TEST(SlippageTest, BuysPayMore_SellsReceiveLess) {
    const PriceHistory history = loadAapl();
    SlippageModel model(history, 5.0);
    const time_t date = dateOf(history, 25);
    EXPECT_GT(model.fillPrice(date, 100.0, OrderSide::BUY), 100.0);
    EXPECT_LT(model.fillPrice(date, 100.0, OrderSide::SELL), 100.0);
}

TEST(SlippageTest, BaseOnly_WhenNoVolatilityHistory) {
    // Bar 0 has no earlier returns, so only the base cost applies: 5 bps
    const PriceHistory history = loadAapl();
    SlippageModel model(history, 5.0, 0.1);
    EXPECT_DOUBLE_EQ(model.slippageFraction(dateOf(history, 0)), 0.0005);
}

TEST(SlippageTest, ConstantPrices_NoVolatilityCost) {
    PriceHistory history("CONST");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/constant_price.csv");
    SlippageModel model(history, 5.0, 1.0);
    EXPECT_DOUBLE_EQ(model.slippageFraction(dateOf(history, 9)), 0.0005);
}

TEST(SlippageTest, HigherVolatilityMultiplier_HigherCost) {
    const PriceHistory history = loadAapl();
    SlippageModel calm(history, 5.0, 0.0);
    SlippageModel sensitive(history, 5.0, 0.5);
    const time_t date = dateOf(history, 25);
    EXPECT_GT(sensitive.slippageFraction(date), calm.slippageFraction(date));
}

TEST(SlippageTest, UsesOnlyEarlierBars) {
    // Changing the fill bar's own price must not change its slippage
    PriceHistory a("A");
    PriceHistory b("B");
    const double closes[] = {100, 102, 99, 103, 101, 104};
    for (size_t i = 0; i < 6; ++i) {
        const time_t date = convertDateToEpoch("2023-01-0" + std::to_string(i + 2));
        a.addDataPoint(PricePoint(date, closes[i]));
        b.addDataPoint(PricePoint(date, i == 5 ? 150.0 : closes[i]));
    }
    SlippageModel modelA(a, 5.0, 0.2);
    SlippageModel modelB(b, 5.0, 0.2);
    const time_t lastDate = a.getDataPoint(static_cast<size_t>(5)).getDate();
    EXPECT_DOUBLE_EQ(modelA.slippageFraction(lastDate), modelB.slippageFraction(lastDate));
}

TEST(SlippageTest, RoundTripCost_IsPositive) {
    const PriceHistory history = loadAapl();
    SlippageModel model(history, 5.0);
    const double cost =
        model.roundTripCost(dateOf(history, 10), 130.0, dateOf(history, 20), 140.0, 100.0);
    // At least the base cost on both legs: 100 * (130 + 140) * 0.0005
    EXPECT_GE(cost, 100.0 * (130.0 + 140.0) * 0.0005);
}

TEST(SlippageTest, UnknownDate_Throws) {
    const PriceHistory history = loadAapl();
    SlippageModel model(history, 5.0);
    EXPECT_THROW((void)model.slippageFraction(convertDateToEpoch("2099-01-01")),
                 std::invalid_argument);
}

TEST(SlippageTest, InvalidParameters_Throw) {
    const PriceHistory history = loadAapl();
    EXPECT_THROW(SlippageModel(history, -1.0), std::invalid_argument);
    EXPECT_THROW(SlippageModel(history, 5.0, -0.1), std::invalid_argument);
    EXPECT_THROW(SlippageModel(history, 5.0, 0.1, 1), std::invalid_argument);
}
