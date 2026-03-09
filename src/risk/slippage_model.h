#pragma once

#include <vector>

#include "core/price_history.h"

namespace barcola {

class SlippageModel {
public:
    SlippageModel(const PriceHistory& priceHistory, double slippagePercentage);

    [[nodiscard]] double calculateSlippage(time_t entryDate, double entryPrice, time_t exitDate,
                                           double exitPrice, double tradeVolume) const;

private:
    const PriceHistory& priceHistory_;
    double slippagePercentage_;

    [[nodiscard]] double calculateActualPrice(double referencePrice,
                                              double requestedPrice) const;
    [[nodiscard]] double calculateVolumeFactor(double tradeVolume) const;
    [[nodiscard]] double calculateMarketVolatility() const;
    [[nodiscard]] double calculateVolatilityAdjustment(double marketVolatility) const;
};

}  // namespace barcola
