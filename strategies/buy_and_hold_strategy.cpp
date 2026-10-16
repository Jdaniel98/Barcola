#include "buy_and_hold_strategy.h"

namespace barcola {

Signal BuyAndHoldStrategy::evaluate(const BarView& /*bars*/, Position position) {
    return position == Position::FLAT ? Signal::BUY : Signal::HOLD;
}

std::string BuyAndHoldStrategy::name() const {
    return "BuyAndHold";
}

}  // namespace barcola
