#pragma once

#include <string>
#include <vector>
#include "core/price_point.h"

namespace barcola {

enum class Signal { BUY, SELL, HOLD };

struct Trade {
    std::string symbol;
    double entryPrice = 0.0;
    double exitPrice = 0.0;
    std::string entryDate;
    std::string exitDate;
    double shares = 0.0;
    double pnl = 0.0;
    double returnPct = 0.0;
};

class Strategy {
public:
    virtual ~Strategy() = default;

    virtual Signal evaluate(const std::vector<PricePoint>& history,
                            size_t currentIndex) = 0;

    [[nodiscard]] virtual std::string name() const = 0;
};

}  // namespace barcola
