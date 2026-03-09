# Barcola — Quantitative Trading Toolkit in C++17

A modular quantitative analysis and backtesting framework built from scratch in modern C++. No external trading library dependencies.

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

**Requirements:** C++17 compiler, CMake 3.16+, libcurl

## Project Structure

```
src/
├── core/           PricePoint, PriceHistory (CSV + Yahoo Finance), time utilities
├── indicators/     SMA, EMA, RSI, Bollinger Bands
├── analysis/       Mean reversion, correlation, market data analysis
├── risk/           Position sizing, slippage modeling, dynamic hedging
└── simulation/     Monte Carlo simulation
tests/              Google Test suite (14 test files, 60+ test cases)
data/               Static CSV fixtures for reproducible testing
```

## Modules

| Module | Description |
|--------|-------------|
| **Core** | OHLCV data structures, Yahoo Finance API integration, CSV import/export, date utilities |
| **Indicators** | Simple & exponential moving averages, RSI, Bollinger Bands |
| **Analysis** | Mean reversion detection, Pearson correlation, volatility & trend analysis |
| **Risk** | Position sizing (risk-based & max-loss), slippage modeling with volume tiers, dynamic hedging |
| **Simulation** | Monte Carlo simulation with dual-SMA crossover strategy |

## Data Loading

```cpp
#include "core/price_history.h"

barcola::PriceHistory history("AAPL");

// From CSV (for reproducible testing and backtesting)
history.loadFromCsv("data/AAPL_2023_sample.csv");

// From Yahoo Finance API (live data)
history.fetchHistoricalData("2023-01-01", "2023-12-31", "1d");

// Save fetched data for later use
history.saveToCsv("data/aapl_2023.csv");
```

## Quick Example

```cpp
#include "core/price_history.h"
#include "indicators/rsi.h"
#include "analysis/correlation.h"

using namespace barcola;

PriceHistory aapl("AAPL");
aapl.loadFromCsv("data/AAPL_2023_sample.csv");

// Calculate RSI
std::vector<double> prices;
for (const auto& dp : aapl.getDataPoints()) {
    prices.push_back(dp.getClosing());
}
double rsi = calculateRSI(prices, 14);

// Correlation between two assets
PriceHistory msft("MSFT");
msft.loadFromCsv("data/MSFT_2023_sample.csv");
CorrelationAnalysis corr(aapl, msft);
double correlation = corr.calculateCorrelation();
```

## Roadmap

- [x] **Phase 1:** Foundation hardening (CMake, tests, CI, bug fixes, CSV support)
- [ ] **Phase 2:** Backtesting engine (strategy interface, engine, performance metrics)
- [ ] **Phase 3:** Mean reversion backtest with documented results
- [ ] **Phase 4:** Second strategy (dual MA crossover) + comparison
- [ ] **Phase 5:** Polish, visualizations, results-first README
