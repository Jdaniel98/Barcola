// Prints every value the strategies' decisions depend on, one per bar, in hexfloat so nothing
// is lost to rounding. Comparing two builds' output (scripts/compare_decision_values.py) shows
// how far a compiler setting moves those values, which is how Phase 9c set its threshold.
//
// Series: each symbol in data_dir, plus a long random walk built from raw mt19937 integers, with
// no maths-library calls, so the walk itself is the same in every build.
// Parameters: the Phase 3-4 grids, which are also the Phase 7 grids and contain its choices.
//
// Usage: decision_values [data_dir] > values.txt
//   data_dir  folder of <SYMBOL>.csv files (default: data/golden)

#include <cstdint>
#include <ctime>
#include <exception>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "detail/window_stats.h"
#include "preregistration.h"
#include "study_common.h"

using namespace barcola;
using namespace barcola::study;

namespace {

constexpr size_t WALK_BARS = 1000000;
constexpr std::uint32_t WALK_SEED = 9;
constexpr time_t SECONDS_PER_DAY = 86400;

// The walk moves in integer ticks of 0.0001, so no rounding error can build up in it
constexpr double TICKS_PER_UNIT = 10000.0;
constexpr std::int64_t WALK_START_TICKS = 1000000;
constexpr std::int64_t WALK_MAX_STEP_TICKS = 2000;
constexpr std::int64_t WALK_FLOOR_TICKS = 100000;  // reflects here, so prices stay positive

struct Windows {
    std::set<size_t> lookbacks;
    std::set<std::pair<size_t, size_t>> crossovers;
    size_t trend = phase7::TREND_WINDOW;
};

Windows gridWindows() {
    Windows windows;
    for (const Family& family : phase7::families()) {
        for (const Params& params : family.grid) {
            if (family.name == "MACrossover") {
                windows.crossovers.emplace(static_cast<size_t>(params[0]),
                                           static_cast<size_t>(params[1]));
            } else {
                windows.lookbacks.insert(static_cast<size_t>(params[0]));
            }
        }
    }
    return windows;
}

std::vector<PricePoint> randomWalk() {
    std::mt19937 generator(WALK_SEED);
    const auto range = static_cast<std::uint32_t>(2 * WALK_MAX_STEP_TICKS + 1);
    std::vector<PricePoint> points;
    points.reserve(WALK_BARS);
    std::int64_t ticks = WALK_START_TICKS;
    for (size_t i = 0; i < WALK_BARS; ++i) {
        ticks += static_cast<std::int64_t>(generator() % range) - WALK_MAX_STEP_TICKS;
        if (ticks < WALK_FLOOR_TICKS) {
            ticks = 2 * WALK_FLOOR_TICKS - ticks;
        }
        points.emplace_back(static_cast<time_t>(i) * SECONDS_PER_DAY,
                            static_cast<double>(ticks) / TICKS_PER_UNIT);
    }
    return points;
}

// Runs value(bars) on every prefix of the series that holds at least `window` bars.
template <typename Value>
void printSeries(const std::string& header, const std::vector<PricePoint>& points, size_t window,
                 Value value) {
    std::cout << "# " << header << "\n";
    for (size_t count = window; count <= points.size(); ++count) {
        std::cout << value(BarView(points, count)) << "\n";
    }
}

void printAll(const std::string& series, const std::vector<PricePoint>& points,
              const Windows& windows) {
    for (const size_t lookback : windows.lookbacks) {
        printSeries(series + " z lookback=" + std::to_string(lookback), points, lookback,
                    [&](const BarView& bars) { return detail::zScore(bars, lookback); });
    }
    for (const auto& [shortWindow, longWindow] : windows.crossovers) {
        printSeries(series + " ma_gap short=" + std::to_string(shortWindow) +
                        " long=" + std::to_string(longWindow),
                    points, longWindow, [&, s = shortWindow, l = longWindow](const BarView& bars) {
                        return detail::relativeGap(detail::trailingMean(bars, s),
                                                   detail::trailingMean(bars, l));
                    });
    }
    printSeries(series + " trend_gap window=" + std::to_string(windows.trend), points,
                windows.trend, [&](const BarView& bars) {
                    return detail::relativeGap(bars.back().getClosing(),
                                               detail::trailingMean(bars, windows.trend));
                });
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const fs::path dataDir = argc > 1 ? argv[1] : "data/golden";
        const Windows windows = gridWindows();
        std::ios::sync_with_stdio(false);
        std::cout << std::hexfloat;
        for (const PriceHistory& history : loadUniverse(dataDir)) {
            printAll(history.getAssetSymbol(), history.getDataPoints(), windows);
        }
        printAll("random_walk", randomWalk(), windows);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
