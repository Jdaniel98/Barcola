# Phase 4: Moving-Average Crossover vs Mean Reversion

**Question.** Phase 3 found that buying dips did not beat holding. Does the opposite approach, following trends, do better? And how do the two compare with each other?

**Answer.** Out of sample (2020–2024), trend following did much better than buying dips: 13.9% a year with a Sharpe ratio of 0.65, against 4.5% and 0.30. It came close to buy-and-hold on a risk-adjusted basis (0.65 against 0.67) with smaller drawdowns, but it still earned less, and one stock, XOM, accounts for much of its edge.

The bigger lesson is that the in-sample ranking was the other way round. On 2014–2019, mean reversion looked like the better strategy (Sharpe 0.75 against 0.59). Choosing between strategies on one calm period would have picked the one that did worse next.

All numbers come from [`results/study_output.md`](../results/study_output.md), produced by `build/apps/strategy_study`. See [Reproducing](#reproducing).

## The strategy

`MovingAverageCrossoverStrategy` is long while the short moving average of the close is above the long one, and flat while it is below. It follows the current state rather than waiting for a fresh cross. So if a test window opens mid-uptrend, it buys on the first day instead of sitting out the trend.

Everything else matches [Phase 3](phase-3-results.md#setup): same six symbols, windows, costs, next-day fills and all-cash sizing, no interest on cash, and buy-and-hold as the benchmark.

**Parameter selection.** Eight pairs: short ∈ {10, 20, 50} days, long ∈ {50, 100, 200} days, short below long. As with mean reversion, one pair was chosen for all six symbols by the highest average Sharpe ratio on 2014–2019: **20-day over 200-day**.

**Data and warm-up.** A 200-day average needs 200 days of history before the first trade, so the data now starts in January 2012 and every run gets one year (252 trading days) of history before its window. This does not change the Phase 3 results: mean reversion looks back at most 50 days, and its result files came out byte-identical.

## Results

### Side by side, average of six symbols

| 2014–2019, in-sample | CAGR | Sharpe | Max drawdown | Trades | Win rate | In market |
|---|---:|---:|---:|---:|---:|---:|
| Mean reversion (20-day, z 2.0 / 0.5) | 7.5% | **0.75** | **15.4%** | 26.0 | 80.6% | 23% |
| MA crossover (20 / 200) | 10.4% | 0.59 | 24.7% | 5.8 | 59.5% | 76% |
| Buy-and-hold | **15.6%** | 0.81 | 24.1% | 1 | | 100% |

| 2020–2024, out-of-sample | CAGR | Sharpe | Max drawdown | Trades | Win rate | In market |
|---|---:|---:|---:|---:|---:|---:|
| Mean reversion (20-day, z 2.0 / 0.5) | 4.5% | 0.30 | 33.7% | 19.8 | 64.8% | 24% |
| MA crossover (20 / 200) | 13.9% | **0.65** | **30.8%** | 4.5 | 45.8% | 76% |
| Buy-and-hold | **16.7%** | 0.67 | 39.6% | 1 | | 100% |

### MA crossover by symbol, out-of-sample

| Symbol | MA CAGR | MA Sharpe | MA max DD | Trades | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | 18.3% | 0.74 | 34.7% | 5 | 82.0% | 28.3% | 0.95 | 31.4% |
| JPM | 14.2% | 0.71 | 36.6% | 3 | 71.1% | 14.9% | 0.59 | 43.1% |
| KO | −0.8% | 0.03 | 33.0% | 5 | 73.9% | 5.9% | 0.38 | 37.0% |
| MSFT | 22.0% | 0.89 | 28.0% | 6 | 79.5% | 22.7% | 0.82 | 37.1% |
| SPY | 8.6% | 0.60 | 32.2% | 4 | 77.9% | 14.5% | 0.75 | 33.7% |
| XOM | 21.4% | 0.92 | 20.5% | 4 | 69.6% | 13.9% | 0.55 | 55.3% |

The crossover beat buy-and-hold's Sharpe ratio on three symbols (JPM, MSFT, XOM) and lost on three (AAPL, KO, SPY).

### How much the parameter choice matters

- The chosen 20/200 pair ranked **2nd of 8** out of sample (Sharpe 0.65; the best, 10/200, scored 0.68).
- The three pairs using a 200-day average were the top three **in both periods**. Slower averages worked better, and that held up.
- **8 of 8** pairs made money out of sample, and **1 of 8** beat buy-and-hold's average Sharpe ratio.

For comparison, the chosen mean-reversion set ranked 7th of 32 and none of its 32 beat buy-and-hold ([Phase 3](phase-3-results.md#how-much-the-parameter-choice-matters)).

## What the comparison shows

**1. The in-sample ranking reversed.** On 2014–2019, a calm and steadily rising market, buying small dips worked well and trends were mild, so mean reversion looked better. 2020–2024 had a crash, a bear market in 2022 and long trends in both directions, which suits trend following. A strategy chosen because it did best in one kind of market can do worst in the next.

**2. The two strategies make money in opposite ways.**

| Held | MR trades | MR win rate | MR avg return | MA trades | MA win rate | MA avg return |
|---|---:|---:|---:|---:|---:|---:|
| 0–14 days | 49 | 93.9% | +5.6% | 4 | 0.0% | −2.4% |
| 15–30 days | 42 | 71.4% | +2.1% | 2 | 0.0% | −4.0% |
| 31–60 days | 26 | 7.7% | −5.1% | 1 | 0.0% | −13.0% |
| 61+ days | 2 | 0.0% | −21.4% | 20 | 60.0% | +32.0% |

*(Out-of-sample trades. In-sample shows the same pattern.)*

Mean reversion wins often and small, and its rare long trades are its big losses. The crossover loses on every short trade, which are false signals that reverse within weeks, and makes all its money on a few long trends. Each strategy's weakness is the other's strength.

**3. Trend following avoids long declines, not fast crashes.** The 200-day average reacts slowly. In March 2020 the crossover sold SPY only after a 23.8% loss, JPM after 30.3% and KO after 22.2%. It then bought SPY back in June, after much of the rebound. Where it helped was XOM. It held no XOM at all during the 2020 energy collapse, then bought in December 2020 and held until June 2023 for a 174% gain. Its worst drawdown on XOM was 20.5%, against buy-and-hold's 55.3%.

**4. One stock carries much of the edge.** Without XOM, the crossover averages 12.5% a year with a Sharpe of 0.59 and a 32.9% drawdown, against buy-and-hold's 17.3%, 0.70 and 36.5%. With only 3–6 trades per symbol, each symbol's result depends on a handful of trends.

**5. Combining them diversifies only a little.** The two strategies' daily returns have an average correlation of 0.47. Both are long the same stock, so whenever both hold it they move together.

## Caveats

Everything in [Phase 3's caveats](phase-3-results.md#caveats) applies: symbols picked with hindsight, no interest on cash, one symbol per backtest, and simple cost assumptions. Two more:

- **Very few trades.** The crossover made 27 out-of-sample trades across all six symbols. Its per-symbol results are anecdotes; only the averages mean much.
- **The 2020–2024 data has now been used twice.** Phase 3 looked at it, and so did this comparison. Any new idea built on what these two studies found needs testing on data neither has seen, such as 2025 or a different set of symbols.

## What to try next

1. **Combine the two ideas.** Buy dips only while the 200-day trend is up. That would let mean reversion avoid the long declines that caused its worst trades.
2. **A time stop for mean reversion**, as suggested in Phase 3.
3. **Charts** for Phase 5: equity curves, drawdowns and trade timelines are already saved in `results/equity_out_of_sample.csv` and `results/trades.csv`.

## Reproducing

```bash
scripts/fetch_market_data.sh                       # needs a free Tiingo API token
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
build/apps/strategy_study data/market results
```

| File | Contents | In git |
|---|---|---|
| `results/study_output.md` | All tables in this document and in Phase 3 | Yes |
| `results/summary.csv` | Every metric for the chosen parameters of each strategy and buy-and-hold, both periods | Yes |
| `results/<strategy>_in_sample_grid.csv`, `results/<strategy>_out_of_sample_grid.csv` | Every metric for every parameter set, per symbol, for `mean_reversion` and `ma_crossover` | Yes |
| `results/trades.csv` | Every trade of the chosen parameters, with fill prices | No |
| `results/equity_out_of_sample.csv` | Daily equity for every strategy, 2020–2024 | No |
