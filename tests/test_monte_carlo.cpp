#include <gtest/gtest.h>
#include "simulation/monte_carlo.h"

using namespace barcola;

TEST(MonteCarloTest, InsufficientData_ReturnsZero) {
    PriceHistory history("UP");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/monotonic_up.csv");

    // Only 16 data points, need 52+ for SMA(50)
    double result = performMonteCarloSimulation(history, 10, 100000.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(MonteCarloTest, EmptyData_ReturnsZero) {
    PriceHistory history("EMPTY");
    history.loadFromCsv(std::string(BARCOLA_TEST_DATA_DIR) + "/empty.csv");

    double result = performMonteCarloSimulation(history, 10, 100000.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}
