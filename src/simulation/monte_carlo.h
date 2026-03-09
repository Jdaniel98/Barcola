#pragma once

#include <cstddef>

#include "core/price_history.h"

namespace barcola {

// Run a Monte Carlo simulation using dual SMA crossover strategy.
// Returns average profit across all simulations.
[[nodiscard]] double performMonteCarloSimulation(const PriceHistory& history,
                                                 size_t numSimulations,
                                                 double initialInvestment);

}  // namespace barcola
