# Barcola

A quantitative trading toolkit written from scratch in C++17, used to answer one question: **do two classic trading strategies, buying sharp dips and following trends, beat simply holding the stock?**

On six large US stocks and ETFs, with parameters chosen on 2014–2019 and tested unchanged on 2020–2024, after trading costs: **no.** Following trends came close on a risk-adjusted basis; buying dips fell well short.

![Growth of $100,000 in each strategy, 2020–2024](results/charts/growth.svg)

| 2020–2024, out of sample, average of 6 symbols | CAGR | Sharpe | Max drawdown | Time in market |
|---|---:|---:|---:|---:|
| Mean reversion: buy dips 2 standard deviations below the 20-day average | 4.5% | 0.30 | 33.7% | 24% |
| MA crossover: hold while the 20-day average is above the 200-day | 13.9% | 0.65 | 30.8% | 76% |
| Buy-and-hold | 16.7% | 0.67 | 39.6% | 100% |

## What the study found

**Buying dips lost to holding, whichever settings were used.** None of the 32 mean-reversion parameter sets beat buy-and-hold out of sample, and neither did 7 of the 8 crossover sets. Mean reversion wins often and small: dips that recovered within two weeks won 94% of the time. But it never cuts a loss, and the trades still open after a month were mostly losers. [Phase 3 write-up](docs/phase-3-results.md)

![Every parameter set's out-of-sample Sharpe ratio against buy-and-hold](results/charts/parameter_sensitivity.svg)

**The selection period pointed the wrong way.** On 2014–2019, a calm and rising market, mean reversion had the better Sharpe ratio. On 2020–2024, with a crash, a bear market and long trends, the ranking reversed. Choosing between strategies on one kind of market would have picked the one that did worse next. [Phase 4 write-up](docs/phase-4-results.md)

![Sharpe ratio of each strategy in the selection period and the test period](results/charts/sharpe_in_vs_out.svg)

**Trend following limited drawdowns, but not in fast crashes.** The crossover's averaged accounts fell 24.5% at worst, against 35.8% for buy-and-hold, but most of that came in March 2020, before a 200-day average could react. Much of its edge comes from one stock: it held no XOM through the 2020 oil collapse and then rode the recovery for a 174% gain.

![Drawdown from peak for each strategy, 2020–2024](results/charts/drawdown.svg)

## How the test was run

| | |
|---|---|
| Symbols | SPY, AAPL, MSFT, JPM, XOM, KO, each backtested on its own with $100,000 |
| Data | Daily prices from Tiingo, adjusted for splits and dividends |
| Selection | One parameter set per strategy, chosen by average Sharpe ratio on 2014–2019 |
| Test | The chosen set run unchanged on 2020–2024, next to buy-and-hold |
| Fills | Decide on the close, fill at the next day's open |
| Costs | 0.1% commission and 5 bps slippage on every buy and sell |
| Look-ahead | Strategies only ever see bars up to the current one |

**Caveats.** The symbols are well-known companies chosen with hindsight, which flatters buy-and-hold. Idle cash earns no interest. Each symbol has only 16–30 mean-reversion trades and 3–8 crossover trades per period, so the averages across symbols mean more than any single row. And the 2020–2024 period has now been used by both studies, so any new idea built on these findings needs testing on data neither has seen. Details are in the write-ups.

## Reproducing the results

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
scripts/fetch_market_data.sh                   # needs a free Tiingo API token
build/apps/strategy_study data/market results
```

The study prints every table in the write-ups to `results/study_output.md` and redraws the charts in `results/charts/`. A rerun reproduces the committed results exactly. Tiingo's terms restrict redistributing its prices, so the downloaded data and the outputs that contain prices (trades and daily equity) are kept out of git.

**Requirements:** a C++17 compiler, CMake 3.16+ and libcurl. The download script needs curl and python3.

## The toolkit

| Module | What it does |
|---|---|
| **Core** | Daily bars kept in date order; CSV import and export with split and dividend adjustment; Yahoo Finance download; date utilities |
| **Indicators** | Simple and exponential moving averages, Wilder's RSI, Bollinger Bands |
| **Analysis** | Deviation from a moving average, return correlation between assets (aligned by date), annualized volatility |
| **Risk** | Stop-based position sizing with a leverage cap, a volatility-based slippage model |
| **Simulation** | Monte Carlo test of a moving-average crossover on bootstrapped price paths |
| **Backtest** | Long-only engine with next-open fills, commission, slippage, warm-up history and interest on idle cash; Sharpe, Sortino, max drawdown, Calmar and trade statistics |
| **Strategies** | Z-score mean reversion, moving-average crossover, buy-and-hold |
| **Report** | SVG line, panel, dumbbell and strip charts with light and dark themes |

```
src/          core/ indicators/ analysis/ risk/ simulation/ backtest/ report/
strategies/   Strategy implementations
apps/         strategy_study: the study behind the results above
scripts/      fetch_market_data.sh: Tiingo download
docs/         Study write-ups
results/      Study outputs and charts
tests/        Google Test suite, one file per module
data/         Small CSV fixtures for tests
```

### Running a backtest

```cpp
#include "backtest/backtest_engine.h"
#include "ma_crossover_strategy.h"

using namespace barcola;

PriceHistory spy("SPY");
spy.loadFromCsv("data/market/SPY.csv");

BacktestConfig config;            // defaults shown
config.initialCapital = 100000.0;
config.commissionRate = 0.001;    // 0.1% of each trade's value
config.slippageBps = 5.0;
config.positionFraction = 1.0;    // share of cash put into each entry
config.riskFreeRate = 0.05;       // earned on idle cash; Sharpe/Sortino hurdle
config.fillTiming = FillTiming::NEXT_OPEN;
config.warmupBars = 0;            // leading bars used only as history

MovingAverageCrossoverStrategy strategy(20, 200);
BacktestResult result = BacktestEngine(config).run(strategy, spy);
// result.annualizedReturn, result.sharpeRatio, result.maxDrawdown, result.trades, ...
```

A strategy implements `evaluate(bars, position)`. It is called after each close, sees only the bars up to that day, and is told whether it is currently holding a position, so it never has to track that itself. Orders fill at the next day's open; `FillTiming::SAME_CLOSE` fills at the signal day's close instead, for comparison.

### Loading data and using indicators

```cpp
#include "analysis/correlation.h"
#include "core/price_history.h"
#include "indicators/rsi.h"

using namespace barcola;

PriceHistory aapl("AAPL");
aapl.loadFromCsv("data/AAPL_2023_sample.csv");

// Latest RSI(14); empty if there are fewer than 15 prices
std::optional<double> rsi = calculateRSI(aapl.getClosingPrices(), 14);

// Correlation of daily returns between two assets, on the dates both have
PriceHistory msft("MSFT");
msft.loadFromCsv("data/MSFT_2023_sample.csv");
double correlation = CorrelationAnalysis(aapl, msft).calculateCorrelation();
```

CSV columns are matched by header name (`Date`, `Open`, `High`, `Low`, `Close`). When an `Adj Close` column is present, all four prices are scaled by `Adj Close / Close`. Each load replaces any bars already loaded, rows are sorted by date, and duplicate dates or malformed numbers raise an error naming the file and line. `fetchHistoricalData` downloads from Yahoo Finance, but Yahoo now restricts that endpoint, so expect an HTTP error and prefer CSV files.

## Engineering

- **197 tests in 20 programs** (Google Test), run in CI on Ubuntu and macOS. Every bug fix comes with a regression test.
- **Known-value tests for the math.** RSI, correlation, volatility, drawdown, annualized return, Bollinger Bands and the backtest's fills and sizing are checked against hand-calculated or independently computed values.
- **Built to avoid backtest traps.** Strategies cannot see future bars, fills happen the next day, costs apply to every trade, and the engine, not the strategy, is the record of what is held.
- **Reproducible.** Same data, same results, byte for byte. The Monte Carlo simulation uses a fixed seed and a standard-defined generator.

## Roadmap

- [x] **Phase 1:** Foundation hardening (CMake, tests, CI, bug fixes, CSV support)
- [x] **Phase 2:** Backtesting engine (strategy interface, engine, performance metrics)
- [x] **Phase 3:** Mean reversion backtest with documented results ([write-up](docs/phase-3-results.md))
- [x] **Phase 4:** Second strategy (dual MA crossover) + comparison ([write-up](docs/phase-4-results.md))
- [x] **Phase 5:** Charts and a results-first README
