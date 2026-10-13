#include "simulation/monte_carlo.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>

#include "indicators/moving_average.h"

namespace barcola {

namespace {

constexpr double INVEST_FRACTION = 0.2;

// Linear interpolation between the closest ranks of an ascending vector.
double percentile(const std::vector<double>& sorted, double fraction) {
    const double position = fraction * static_cast<double>(sorted.size() - 1);
    const size_t lower = static_cast<size_t>(std::floor(position));
    const size_t upper = std::min(lower + 1, sorted.size() - 1);
    const double weight = position - static_cast<double>(lower);
    return sorted[lower] + weight * (sorted[upper] - sorted[lower]);
}

}  // namespace

double runSmaCrossover(const std::vector<double>& closes, double initialInvestment) {
    if (closes.size() < MONTE_CARLO_MIN_BARS) {
        return 0.0;
    }

    const std::vector<double> shortMA = calculateSMA(closes, CROSSOVER_SHORT_WINDOW);
    const std::vector<double> longMA = calculateSMA(closes, CROSSOVER_LONG_WINDOW);

    double cash = initialInvestment;
    double sharesOwned = 0.0;

    // Days before both averages exist hold NaN, which makes every comparison false.
    for (size_t day = 1; day < closes.size(); ++day) {
        // FIX: these two conditions were swapped, so the rule bought on death crosses
        const bool goldenCross = shortMA[day - 1] <= longMA[day - 1] && shortMA[day] > longMA[day];
        const bool deathCross = shortMA[day - 1] >= longMA[day - 1] && shortMA[day] < longMA[day];

        if (goldenCross) {
            const double amountToInvest = cash * INVEST_FRACTION;
            sharesOwned += amountToInvest / closes[day];
            cash -= amountToInvest;
        } else if (deathCross && sharesOwned > 0.0) {
            cash += sharesOwned * closes[day];
            sharesOwned = 0.0;
        }
    }

    return cash + sharesOwned * closes.back() - initialInvestment;
}

// FIX: every simulation used to replay the same historical prices, so all N runs were
// identical. Each one now runs on a different bootstrapped price path.
MonteCarloResult performMonteCarloSimulation(const PriceHistory& history, size_t numSimulations,
                                             double initialInvestment, std::uint32_t seed) {
    if (numSimulations == 0) {
        throw std::invalid_argument("numSimulations must be at least 1");
    }
    if (!(initialInvestment > 0.0)) {
        throw std::invalid_argument("initialInvestment must be positive");
    }

    MonteCarloResult result;
    const std::vector<double> closes = history.getClosingPrices();
    if (closes.size() < MONTE_CARLO_MIN_BARS) {
        return result;
    }

    std::vector<double> logReturns;
    logReturns.reserve(closes.size() - 1);
    for (size_t i = 1; i < closes.size(); ++i) {
        if (closes[i] <= 0.0 || closes[i - 1] <= 0.0) {
            throw std::invalid_argument("Monte Carlo simulation needs positive prices");
        }
        logReturns.push_back(std::log(closes[i] / closes[i - 1]));
    }

    // std::mt19937 output is fixed by the standard, unlike std::uniform_int_distribution,
    // so drawing indices with % keeps results identical across compilers. The modulo bias
    // is negligible for a few thousand returns against a 2^32 range.
    std::mt19937 generator(seed);
    std::vector<double> path(closes.size());
    result.profits.reserve(numSimulations);

    for (size_t simulation = 0; simulation < numSimulations; ++simulation) {
        path[0] = closes[0];
        for (size_t day = 1; day < path.size(); ++day) {
            const size_t draw = static_cast<size_t>(generator()) % logReturns.size();
            path[day] = path[day - 1] * std::exp(logReturns[draw]);
        }
        result.profits.push_back(runSmaCrossover(path, initialInvestment));
    }

    std::sort(result.profits.begin(), result.profits.end());
    result.meanProfit = std::accumulate(result.profits.begin(), result.profits.end(), 0.0) /
                        static_cast<double>(result.profits.size());
    result.medianProfit = percentile(result.profits, 0.5);
    result.percentile5 = percentile(result.profits, 0.05);
    result.percentile95 = percentile(result.profits, 0.95);
    return result;
}

}  // namespace barcola
