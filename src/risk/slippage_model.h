#pragma once

#include <cstddef>
#include <ctime>

#include "core/price_history.h"

namespace barcola {

enum class OrderSide { BUY, SELL };

// Deterministic slippage: every fill loses a fixed base cost plus a cost proportional to
// recent volatility. Volatility is the standard deviation of daily log returns over the
// `volatilityWindow` returns that end at the bar before the fill, so it never uses data
// from the fill date or later. Order size is not modelled because bars carry no volume.
class SlippageModel {
public:
    // baseBps: cost on every fill in basis points. volatilityMultiplier: extra cost as a
    // multiple of daily volatility (0.1 at 2% daily volatility adds 20 bps).
    // Throws std::invalid_argument if baseBps < 0, volatilityMultiplier < 0 or
    // volatilityWindow < 2.
    SlippageModel(const PriceHistory& priceHistory, double baseBps,
                  double volatilityMultiplier = 0.1, size_t volatilityWindow = 20);

    // Fraction of the price lost to slippage on `date`.
    // Throws std::invalid_argument if `date` is not a bar in the history.
    [[nodiscard]] double slippageFraction(time_t date) const;

    // Price actually paid (BUY) or received (SELL) for an order quoted at referencePrice.
    [[nodiscard]] double fillPrice(time_t date, double referencePrice, OrderSide side) const;

    // Slippage cost, in currency, of buying `shares` on entryDate and selling them on
    // exitDate at the given quoted prices. Always >= 0.
    [[nodiscard]] double roundTripCost(time_t entryDate, double entryPrice, time_t exitDate,
                                       double exitPrice, double shares) const;

private:
    const PriceHistory& priceHistory_;
    double baseFraction_;
    double volatilityMultiplier_;
    size_t volatilityWindow_;

    [[nodiscard]] double trailingVolatility(size_t barIndex) const;
};

}  // namespace barcola
