#include "risk/slippage_model.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace barcola {

SlippageModel::SlippageModel(const PriceHistory& priceHistory, double slippagePercentage)
    : priceHistory_(priceHistory), slippagePercentage_(slippagePercentage) {}

double SlippageModel::calculateSlippage(time_t entryDate, double entryPrice, time_t exitDate,
                                        double exitPrice, double tradeVolume) const {
    const auto entryDataPoint = priceHistory_.getDataPoint(entryDate);
    const auto exitDataPoint = priceHistory_.getDataPoint(exitDate);

    const double entryActualPrice =
        calculateActualPrice(entryDataPoint.getClosing(), entryPrice);
    const double exitActualPrice =
        calculateActualPrice(exitDataPoint.getClosing(), exitPrice);

    const double slippage = entryActualPrice - exitActualPrice;
    const double volumeFactor = calculateVolumeFactor(tradeVolume);

    return slippage * volumeFactor;
}

double SlippageModel::calculateActualPrice(double referencePrice, double requestedPrice) const {
    const double maxSlippage = referencePrice * slippagePercentage_;
    double actualPrice = requestedPrice;

    if (actualPrice > referencePrice) {
        actualPrice = std::min(referencePrice + maxSlippage, actualPrice);
    } else if (actualPrice < referencePrice) {
        actualPrice = std::max(referencePrice - maxSlippage, actualPrice);
    }

    return actualPrice;
}

double SlippageModel::calculateVolumeFactor(double tradeVolume) const {
    double volumeFactor = 1.0;

    struct VolumeTier {
        double threshold;
        double factor;
    };
    constexpr VolumeTier kVolumeTiers[] = {
        {1000.0, 1.0},
        {10000.0, 0.95},
        {50000.0, 0.9},
        {100000.0, 0.85},
    };

    for (const auto& tier : kVolumeTiers) {
        if (tradeVolume >= tier.threshold) {
            volumeFactor = tier.factor;
        } else {
            break;
        }
    }

    volumeFactor = std::max(volumeFactor, 0.5);

    const double marketVolatility = calculateMarketVolatility();
    volumeFactor *= calculateVolatilityAdjustment(marketVolatility);

    return volumeFactor;
}

double SlippageModel::calculateMarketVolatility() const {
    // Note: Uses random price changes for simulation. In production, this should use
    // actual historical price changes. Preserved from original for backward compatibility.
    constexpr size_t kNumDays = 30;
    std::random_device rd;
    std::mt19937 generator(rd());
    std::normal_distribution<double> distribution(0.0, 0.02);

    std::vector<double> priceChanges;
    priceChanges.reserve(kNumDays);
    for (size_t i = 0; i < kNumDays; ++i) {
        priceChanges.push_back(distribution(generator));
    }

    double sum = 0.0;
    for (const double val : priceChanges) {
        sum += val;
    }
    const double mean = sum / static_cast<double>(kNumDays);

    double squaredDiffSum = 0.0;
    for (const double val : priceChanges) {
        const double diff = val - mean;
        squaredDiffSum += diff * diff;
    }

    return std::sqrt(squaredDiffSum / static_cast<double>(kNumDays));
}

double SlippageModel::calculateVolatilityAdjustment(double marketVolatility) const {
    const double volatilityFactor = 1.0 - 0.5 * std::tanh(marketVolatility);
    return std::max(volatilityFactor, 0.5);
}

}  // namespace barcola
