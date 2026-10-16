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
├── risk/           Position sizing, slippage modeling, moving averages for hedging
├── simulation/     Monte Carlo simulation
└── backtest/       Strategy interface, backtest engine, performance metrics
strategies/         Strategy implementations (mean reversion, buy-and-hold)
apps/               Study programs (mean_reversion_study)
scripts/            Market data download (fetch_market_data.sh)
docs/               Study write-ups
results/            Study outputs
tests/              Google Test suite, one file per module
data/               Static CSV fixtures for reproducible testing
```

## Results: Mean Reversion vs Buy-and-Hold

A z-score mean reversion strategy tested on SPY, AAPL, MSFT, JPM, XOM and KO, with parameters chosen on 2014–2019 and tested unchanged on 2020–2024. Commission, slippage and next-day fills are included.

| 2020–2024, out of sample | CAGR | Sharpe | Max drawdown | Time in market |
|---|---:|---:|---:|---:|
| Mean reversion (average of 6) | 4.5% | 0.30 | 33.7% | 24% |
| Buy-and-hold (average of 6) | 16.7% | 0.67 | 39.6% | 100% |

Buying dips did not beat holding: none of the 32 parameter sets tested did. Dips that recovered within two weeks won 94% of the time, while trades still open after a month mostly lost. See [the full write-up](docs/phase-3-results.md).

## Modules

| Module | Description |
|--------|-------------|
| **Core** | OHLC bars kept in date order, CSV import/export with split and dividend adjustment, Yahoo Finance download, date utilities |
| **Indicators** | Simple and exponential moving average series, Wilder's RSI, Bollinger Bands |
| **Analysis** | Deviation from a moving average, return correlation between assets (aligned by date), annualized volatility, up/down days |
| **Risk** | Stop-based position sizing with a leverage cap, volatility-based slippage model, short/long moving averages for a hedging rule |
| **Simulation** | Monte Carlo test of a dual SMA(20/50) crossover rule on bootstrapped price paths |
| **Backtest** | Long-only engine with next-open fills, commission, slippage, interest on idle cash; Sharpe, Sortino, max drawdown, Calmar, trade statistics |
| **Strategies** | Z-score mean reversion; buy-and-hold benchmark |

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

Each load replaces any bars already in the history. Columns are matched by header name (`Date`, `Open`, `High`, `Low`, `Close`). If an `Adj Close` column is present, all four prices are scaled by `Adj Close / Close`, so splits and dividends don't show up as price jumps. Rows are sorted by date; duplicate dates and malformed numbers raise an error naming the file and line.

Yahoo has restricted its v7 CSV download endpoint, so `fetchHistoricalData` may fail with an HTTP error. When it does, download the CSV another way and use `loadFromCsv`.

## Quick Example

```cpp
#include "core/price_history.h"
#include "indicators/rsi.h"
#include "analysis/correlation.h"

using namespace barcola;

PriceHistory aapl("AAPL");
aapl.loadFromCsv("data/AAPL_2023_sample.csv");

// Latest RSI(14); empty if there are fewer than 15 prices
std::optional<double> rsi = calculateRSI(aapl.getClosingPrices(), 14);

// Correlation of daily returns between two assets, on dates both have
PriceHistory msft("MSFT");
msft.loadFromCsv("data/MSFT_2023_sample.csv");
CorrelationAnalysis corr(aapl, msft);
double correlation = corr.calculateCorrelation();
```

## Backtesting

```cpp
#include "backtest/backtest_engine.h"
#include "mean_reversion_strategy.h"

using namespace barcola;

PriceHistory aapl("AAPL");
aapl.loadFromCsv("data/AAPL_2023_sample.csv");

BacktestConfig config;            // defaults shown
config.initialCapital = 100000.0;
config.commissionRate = 0.001;    // 0.1% of each trade's value
config.slippageBps = 5.0;
config.positionFraction = 1.0;    // share of cash put into each entry
config.riskFreeRate = 0.05;       // earned on idle cash; Sharpe/Sortino hurdle
config.fillTiming = FillTiming::NEXT_OPEN;

MeanReversionStrategy strategy(20, 2.0, 0.0);  // lookback, entry z, exit z
BacktestResult result = BacktestEngine(config).run(strategy, aapl);
```

A strategy decides after each bar's close and is filled at the next bar's open. It sees only bars up to the current one, and the engine tells it whether it is currently long, so the strategy itself doesn't need to track its position. `FillTiming::SAME_CLOSE` fills at the signal bar's close instead, for comparison. Set `config.warmupBars` to give the strategy history before the test window: those bars are visible to it, but trading and every metric start after them.

## Roadmap

- [x] **Phase 1:** Foundation hardening (CMake, tests, CI, bug fixes, CSV support)
- [x] **Phase 2:** Backtesting engine (strategy interface, engine, performance metrics)
- [x] **Phase 3:** Mean reversion backtest with documented results ([write-up](docs/phase-3-results.md))
- [ ] **Phase 4:** Second strategy (dual MA crossover) + comparison
- [ ] **Phase 5:** Polish, visualizations, results-first README
