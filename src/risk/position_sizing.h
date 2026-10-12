#pragma once

namespace barcola {

// Sizes positions so that being stopped out loses a chosen amount, capped so the position
// is never worth more than maxLeverage x the portfolio.
class PositionSizing {
public:
    // riskPercentage in (0, 1], stopLossPercentage in (0, 1), maxLeverage > 0.
    // Throws std::invalid_argument otherwise.
    PositionSizing(double riskPercentage, double stopLossPercentage, double maxLeverage = 1.0);

    // Shares such that a stop-out loses riskPercentage of portfolioSize.
    [[nodiscard]] double calculatePositionSize(double portfolioSize, double entryPrice) const;
    // Shares such that a stop-out loses maxLossAmount.
    [[nodiscard]] double calculatePositionSizeWithMaxLoss(double portfolioSize, double entryPrice,
                                                         double maxLossAmount) const;

    void setRiskPercentage(double percentage);
    void setStopLossPercentage(double percentage);
    [[nodiscard]] double getRiskPercentage() const;
    [[nodiscard]] double getStopLossPercentage() const;
    [[nodiscard]] double getMaxLeverage() const;

private:
    double riskPercentage_;
    double stopLossPercentage_;
    double maxLeverage_;

    [[nodiscard]] double capToLeverage(double shares, double portfolioSize,
                                       double entryPrice) const;
};

}  // namespace barcola
