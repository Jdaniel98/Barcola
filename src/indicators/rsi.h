#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace barcola {

// Wilder's RSI as of the last price. The first `period` changes seed simple averages of gains
// and losses; every later change is folded in with Wilder smoothing (alpha = 1 / period).
//
// Returns std::nullopt with fewer than period + 1 prices. Returns 100 when there are gains but
// no losses, 0 when there are losses but no gains, and 50 when prices never move.
// Throws std::invalid_argument if period is 0.
[[nodiscard]] std::optional<double> calculateRSI(const std::vector<double>& prices, size_t period);

}  // namespace barcola
