#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/price_point.h"

namespace barcola {

enum class Signal { BUY, SELL, HOLD };

// The engine's actual holding. Strategies receive it on every bar instead of tracking it
// themselves, so the two can never disagree.
enum class Position { FLAT, LONG };

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

// Read-only view of the bars a strategy is allowed to see: every bar up to and including the
// one being evaluated (back()). Later bars are not reachable, which rules out look-ahead bias.
class BarView {
public:
    BarView(const std::vector<PricePoint>& bars, size_t count) : data_(bars.data()), size_(count) {
        if (count > bars.size()) {
            throw std::out_of_range("BarView count exceeds the number of bars");
        }
    }

    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }
    [[nodiscard]] const PricePoint& operator[](size_t index) const { return data_[index]; }
    [[nodiscard]] const PricePoint& at(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("BarView index out of range");
        }
        return data_[index];
    }
    [[nodiscard]] const PricePoint& back() const { return at(size_ - 1); }
    [[nodiscard]] const PricePoint* begin() const { return data_; }
    [[nodiscard]] const PricePoint* end() const { return data_ + size_; }

private:
    const PricePoint* data_;
    size_t size_;
};

class Strategy {
public:
    virtual ~Strategy() = default;

    // Called by the engine at the start of every run, so one strategy object can be reused
    // across backtests. Override it if the strategy keeps state between bars.
    virtual void reset() {}

    // Decide what to do after bars.back() closes. `position` is the engine's position going
    // into this decision. The engine ignores BUY while LONG and SELL while FLAT.
    virtual Signal evaluate(const BarView& bars, Position position) = 0;

    [[nodiscard]] virtual std::string name() const = 0;
};

}  // namespace barcola
