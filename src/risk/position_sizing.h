#pragma once

namespace barcola {

class PositionSizing {
public:
    PositionSizing(double riskPercentage, double stopLossPercentage);

    [[nodiscard]] double calculatePositionSize(double portfolioSize, double entryPrice) const;
    [[nodiscard]] double calculatePositionSizeWithMaxLoss(double portfolioSize, double entryPrice,
                                                         double maxLossAmount) const;

    void setRiskPercentage(double percentage);
    void setStopLossPercentage(double percentage);
    [[nodiscard]] double getRiskPercentage() const;
    [[nodiscard]] double getStopLossPercentage() const;

private:
    double riskPercentage_;
    double stopLossPercentage_;
};

}  // namespace barcola
