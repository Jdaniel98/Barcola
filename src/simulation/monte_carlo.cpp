#include "simulation/monte_carlo.h"

namespace barcola {

double performMonteCarloSimulation(const PriceHistory& history, size_t numSimulations,
                                   double initialInvestment) {
    const size_t dataSize = history.dataPointsCount();
    if (dataSize < 52) {
        return 0.0;  // Need at least 51 days for SMA(50) + 1 comparison day
    }

    double totalProfit = 0.0;

    // Pre-compute SMAs once (they don't change between simulations in original code)
    const auto smaShort = history.calculateSMA(20);
    const auto smaLong = history.calculateSMA(50);
    const auto& movingAveragesShort = smaShort.first;
    const auto& movingAveragesLong = smaLong.first;

    const auto& dataPoints = history.getDataPoints();

    for (size_t simulation = 0; simulation < numSimulations; ++simulation) {
        double cash = initialInvestment;
        double sharesOwned = 0.0;

        for (size_t day = 51; day < dataSize; ++day) {
            const bool shouldBuy = movingAveragesShort[day - 1] > movingAveragesLong[day - 1] &&
                                   movingAveragesShort[day] <= movingAveragesLong[day];

            const bool shouldSell = movingAveragesShort[day - 1] < movingAveragesLong[day - 1] &&
                                    movingAveragesShort[day] >= movingAveragesLong[day];

            if (shouldBuy) {
                constexpr double kInvestFraction = 0.2;
                const double amountToInvest = cash * kInvestFraction;
                const double sharesToBuy = amountToInvest / dataPoints[day].getClosing();
                sharesOwned += sharesToBuy;
                cash -= amountToInvest;
            } else if (shouldSell && sharesOwned > 0.0) {
                cash += sharesOwned * dataPoints[day].getClosing();
                sharesOwned = 0.0;
            }
        }

        totalProfit +=
            (cash + sharesOwned * dataPoints[dataSize - 1].getClosing() - initialInvestment);
    }

    return totalProfit / static_cast<double>(numSimulations);
}

}  // namespace barcola
