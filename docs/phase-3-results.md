# Phase 3: Mean Reversion Backtest

**Question.** On large US stocks, does buying unusually sharp dips and selling once the price recovers beat simply holding the stock, after trading costs and realistic fills?

**Answer: no.** From 2020 to 2024, out of sample, the strategy returned 4.5% a year on average with a Sharpe ratio of 0.30. Buy-and-hold returned 16.7% a year with a Sharpe of 0.67. None of the 32 parameter sets tested beat buy-and-hold out of sample. The strategy's one advantage in 2014–2019, shallower drawdowns, did not hold up.

The main weakness is easy to see in the trades. Dips that recover within two weeks are almost always profitable. Dips that are still open after a month are mostly losses, and the strategy has no way to get out of them.

All numbers below come from [`results/study_output.md`](../results/study_output.md), produced by `build/apps/strategy_study`. See [Reproducing](#reproducing). [Phase 4](phase-4-results.md) compares this strategy with a moving-average crossover.

## Setup

| | |
|---|---|
| Strategy | `MeanReversionStrategy`: z-score of the close against its trailing mean and standard deviation. Buy when z < −entry, sell when z > exit. Long only. |
| Benchmark | `BuyAndHoldStrategy`: buy on the first day, hold to the end, same costs. |
| Symbols | SPY, AAPL, MSFT, JPM, XOM, KO. Each is backtested on its own with $100,000. |
| Data | Tiingo daily prices, adjusted for splits and dividends, through 2024-12-31. |
| In-sample | 2014-01-01 to 2019-12-31, used to choose parameters |
| Out-of-sample | 2020-01-01 to 2024-12-31, run once with the chosen parameters |
| Fills | Signal on the close, filled at the next day's open |
| Costs | 0.1% commission and 5 bps slippage on every buy and sell |
| Sizing | All available cash in each trade, whole shares only |
| Cash | Earns no interest. Sharpe uses a zero risk-free rate for both strategies. |
| Warm-up | History before each window, used only to compute the first signals. The strategy looks back at most 50 days. (This study originally used 51 days; Phase 4 raised it to 252 for 200-day averages, and the results here are unchanged.) |

**Parameter selection.** The grid was lookback ∈ {10, 20, 30, 50} days, entry z ∈ {1.0, 1.5, 2.0, 2.5} and exit z ∈ {0.0, 0.5}, which is 32 sets. One set was chosen for all six symbols: the one with the highest average in-sample Sharpe ratio. That was **lookback 20, entry z 2.0, exit z 0.5**. It buys when the close is two standard deviations below its 20-day average and sells once it is half a standard deviation above.

## Results

### In-sample, 2014–2019

| Symbol | MR CAGR | MR Sharpe | MR max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | 3.9% | 0.33 | 32.0% | 21 | 81.0% | 22.1% | 26.7% | 1.09 | 38.5% |
| JPM | 11.2% | 0.93 | 14.1% | 30 | 80.0% | 23.6% | 18.6% | 0.93 | 23.2% |
| KO | 9.1% | 1.22 | 7.0% | 30 | 86.7% | 26.2% | 8.7% | 0.65 | 13.9% |
| MSFT | 13.9% | 1.14 | 7.8% | 21 | 95.2% | 15.9% | 30.1% | 1.28 | 18.2% |
| SPY | 7.5% | 0.85 | 8.9% | 27 | 85.2% | 20.7% | 12.0% | 0.93 | 19.3% |
| XOM | −0.7% | 0.01 | 22.6% | 27 | 55.6% | 31.7% | −2.3% | −0.03 | 31.5% |
| **Average** | **7.5%** | **0.75** | **15.4%** | 26 | 80.6% | 23.3% | **15.6%** | **0.81** | **24.1%** |

In-sample, the strategy looked respectable. Its Sharpe ratio was close to buy-and-hold's, its drawdowns were below buy-and-hold's on every symbol, and four of every five trades won. It earned half the return, but it was only in the market about a quarter of the time.

### Out-of-sample, 2020–2024

| Symbol | MR CAGR | MR Sharpe | MR max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | 4.8% | 0.33 | 25.9% | 18 | 61.1% | 22.3% | 28.3% | 0.95 | 31.4% |
| JPM | 11.9% | 0.61 | 37.8% | 22 | 68.2% | 25.5% | 14.9% | 0.59 | 43.1% |
| KO | −1.3% | −0.01 | 35.7% | 16 | 50.0% | 26.3% | 5.9% | 0.38 | 37.0% |
| MSFT | 11.1% | 0.62 | 21.6% | 23 | 69.6% | 22.3% | 22.7% | 0.82 | 37.1% |
| SPY | 3.2% | 0.28 | 28.6% | 20 | 75.0% | 21.3% | 14.5% | 0.75 | 33.7% |
| XOM | −2.7% | −0.02 | 52.8% | 20 | 65.0% | 28.7% | 13.9% | 0.55 | 55.3% |
| **Average** | **4.5%** | **0.30** | **33.7%** | 20 | 64.8% | 24.4% | **16.7%** | **0.67** | **39.6%** |

Out of sample, the average Sharpe ratio fell from 0.75 to 0.30 and the win rate from 81% to 65%. The drawdown advantage mostly disappeared: 33.7% against 39.6%, even though the strategy held a position only a quarter of the time. JPM is the one symbol where it matched buy-and-hold on a risk-adjusted basis (Sharpe 0.61 against 0.59).

### How much the parameter choice matters

The result is not an artifact of picking one parameter set:

- Out of sample, the chosen set ranked **7th of 32** by average Sharpe ratio (0.30, against a best of 0.35 and a median of 0.25). Choosing in-sample neither helped much nor hurt much.
- **31 of 32** parameter sets still made money out of sample, so the effect is real but small.
- **0 of 32** beat buy-and-hold's average Sharpe ratio of 0.67.

## Why it fell short

**1. It is out of the market most of the time.** The strategy holds a position on about 24% of days. In a decade when these stocks mostly rose, sitting in cash for the other 76% costs more than buying dips gains.

**2. Dips that don't recover quickly turn into large losses.** Grouping trades by how long they were held:

| Held | In-sample trades | Win rate | Avg return | Out-of-sample trades | Win rate | Avg return |
|---|---:|---:|---:|---:|---:|---:|
| 0–14 days | 64 | 100.0% | +3.6% | 49 | 93.9% | +5.6% |
| 15–30 days | 72 | 79.2% | +1.9% | 42 | 71.4% | +2.1% |
| 31–60 days | 18 | 33.3% | −1.7% | 26 | 7.7% | −5.1% |
| 61+ days | 2 | 0.0% | −17.0% | 2 | 0.0% | −21.4% |

The strategy only exits when the price rises back above its average, so a dip that turns into a sustained decline is held all the way down. Five of the eight worst out-of-sample trades were held through the COVID crash of March 2020, four of them bought in the last week of February. The worst was XOM, bought in January 2020 and sold in April for a 38% loss. Out of sample there were more slow trades, and they were worse. That is why the drawdowns grew.

This pattern was already visible in 2014–2019, so a time-based exit is a reasonable next step, not something read off the test period.

## Caveats

- **Symbols chosen with hindsight.** These are well-known companies today. That flatters buy-and-hold, especially for AAPL and MSFT, more than it flatters mean reversion. A fairer universe would be chosen as of 2014.
- **No interest on cash.** The strategy has idle cash about 76% of the time. If that cash had earned 2% a year, it would add roughly 1.5 points to the strategy's annual return. That narrows the gap but does not close it.
- **One symbol at a time.** Each backtest puts all its capital into one symbol. A portfolio that shares capital across symbols would keep more of it invested.
- **Small samples.** About 20 trades per symbol per period. The averages across six symbols are more reliable than any single row.
- **Costs and fills.** 0.1% commission is higher than most US brokers charge today, and 5 bps of slippage may be too little on days when the open gaps after bad news.

## What to try next

1. **A time stop.** Exit any trade that has not reverted after N days, with N chosen in-sample. The 2014–2019 trades support this. The 2020–2024 data has now been looked at, though, so it is no longer a clean test. Any variant should also be checked on data the study has not used yet, such as 2025 or a different set of symbols.
2. **A trend filter.** Only buy dips when the price is above its 200-day average, to avoid buying into sustained declines.
3. **A shared-capital portfolio.** Run all symbols from one account so cash freed by one symbol can be used by another.

## Reproducing

```bash
scripts/fetch_market_data.sh                       # needs a free Tiingo API token
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
build/apps/strategy_study data/market results
```

The study writes:

| File | Contents | In git |
|---|---|---|
| `results/study_output.md` | The tables in this document (under "MeanReversion") | Yes |
| `results/summary.csv` | Every metric for the chosen parameters and buy-and-hold, both periods | Yes |
| `results/mean_reversion_in_sample_grid.csv`, `results/mean_reversion_out_of_sample_grid.csv` | Every metric for all 32 parameter sets, per symbol | Yes |
| `results/trades.csv` | Every trade of the chosen parameters, with fill prices | No |
| `results/equity_out_of_sample.csv` | Daily equity for every strategy, 2020–2024 | No |

The price data in `data/market/` and the two price-level outputs are kept out of git because Tiingo's terms restrict redistributing its data.
