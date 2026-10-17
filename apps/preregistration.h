#pragma once

// Phase 7 pre-registration. Every choice the fresh-data test makes is fixed here and was
// committed before any price data after 2024-12-31 was downloaded: the test window, the
// universes, the strategies, how their parameters were chosen, and the chosen parameters.
// The hypotheses and the rule for judging them are in docs/phase-7-preregistration.md.

#include <memory>
#include <string>
#include <vector>

#include "ma_crossover_strategy.h"
#include "mean_reversion_strategy.h"
#include "study_common.h"

namespace barcola::phase7 {

// Parameters were chosen on these three years, the same rule as the Phase 6 walk-forward ...
inline const study::Window SELECTION{"selection", "Selection", "2022-01-01", "2024-12-31"};
// ... and are tested once on data no earlier phase has seen.
inline const study::Window TEST{"test", "Test", "2025-01-01", "2026-09-30"};

// The two refinements, fixed in advance from what Phases 3-6 found on 2014-2024 data
inline constexpr size_t TREND_WINDOW = 200;   // buy dips only above the 200-day average
inline constexpr size_t TIME_STOP_BARS = 21;  // sell after about one trading month

// Test universes, symbol for symbol. The test refuses a data folder that differs, so no
// symbol can be dropped after the results are known.
inline const std::vector<std::string> SIX_SYMBOLS = {"AAPL", "JPM", "KO", "MSFT", "SPY", "XOM"};
inline const std::vector<std::string> DOW29_SYMBOLS = {
    "AXP", "BA",  "CAT", "CSCO", "CVX", "DIS", "GE",  "GS",  "HD",  "IBM",
    "INTC", "JNJ", "JPM", "KO",  "MCD", "MMM", "MRK", "MSFT", "NKE", "PFE",
    "PG",  "T",   "TRV", "UNH", "UTX", "V",   "VZ",  "WMT", "XOM"};

// The mean-reversion grid of Phases 3-6: lookback x entry z x exit z
inline std::vector<study::Params> meanReversionGrid() {
    std::vector<study::Params> grid;
    for (double lookback : {10.0, 20.0, 30.0, 50.0}) {
        for (double entryZ : {1.0, 1.5, 2.0, 2.5}) {
            for (double exitZ : {0.0, 0.5}) {
                grid.push_back({lookback, entryZ, exitZ});
            }
        }
    }
    return grid;
}

// The four strategies under test, each with its parameter grid
inline std::vector<study::Family> families() {
    const auto meanReversion = [](MeanReversionOptions options) {
        return [options](const study::Params& p) -> std::unique_ptr<Strategy> {
            return std::make_unique<MeanReversionStrategy>(static_cast<size_t>(p[0]), p[1], p[2],
                                                           options);
        };
    };
    const std::vector<std::string> mrParams = {"lookback", "entry_z", "exit_z"};

    study::Family plain{"MeanReversion", "Mean reversion", "MR", "mean_reversion",
                        SeriesColor::First, mrParams, meanReversionGrid(),
                        meanReversion({})};
    study::Family crossover{"MACrossover", "MA crossover", "MA", "ma_crossover",
                            SeriesColor::Second, {"short_window", "long_window"}, {},
                            [](const study::Params& p) -> std::unique_ptr<Strategy> {
                                return std::make_unique<MovingAverageCrossoverStrategy>(
                                    static_cast<size_t>(p[0]), static_cast<size_t>(p[1]));
                            }};
    for (double shortWindow : {10.0, 20.0, 50.0}) {
        for (double longWindow : {50.0, 100.0, 200.0}) {
            if (shortWindow < longWindow) {
                crossover.grid.push_back({shortWindow, longWindow});
            }
        }
    }
    study::Family trend{"MeanReversionTrend", "Mean reversion + trend filter", "MRT",
                        "mean_reversion_trend", SeriesColor::Third, mrParams,
                        meanReversionGrid(), meanReversion({TREND_WINDOW, 0})};
    study::Family timeStop{"MeanReversionTimeStop", "Mean reversion + time stop", "MRS",
                           "mean_reversion_time_stop", SeriesColor::Fourth, mrParams,
                           meanReversionGrid(), meanReversion({0, TIME_STOP_BARS})};
    return {plain, crossover, trend, timeStop};
}

// Parameters chosen by `fresh_data_test select` on SELECTION, before the test data existed
struct Chosen {
    std::string universe;
    std::string family;
    study::Params params;
};

inline const std::vector<Chosen> CHOSEN = {
    {"six", "MeanReversion", {30, 2.5, 0}},
    {"six", "MACrossover", {20, 200}},
    {"six", "MeanReversionTrend", {30, 2, 0}},
    {"six", "MeanReversionTimeStop", {30, 2, 0}},
    {"dow29", "MeanReversion", {50, 1, 0.5}},
    {"dow29", "MACrossover", {20, 200}},
    {"dow29", "MeanReversionTrend", {30, 2.5, 0.5}},
    {"dow29", "MeanReversionTimeStop", {50, 1, 0.5}},
};

}  // namespace barcola::phase7
