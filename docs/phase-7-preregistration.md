# Phase 7 Pre-registration: A Clean Test on Fresh Data

**Status: registered before any price data after 2024-12-31 was downloaded.** This document, the strategy code and the chosen parameters in [`apps/preregistration.h`](../apps/preregistration.h) were committed before the test data existed on this machine. The results write-up will quote the commit and show its date is earlier than the download.

## Why

Every study so far used 2014–2024 data, and 2020–2024 has now been examined by three phases. Ideas built on those findings can't be honestly tested on the same years. Prices from January 2025 onward have never been used. Writing down every choice before collecting them means the test can't be adjusted to fit the results.

## Test data

- **Test window:** 2025-01-01 to 2026-09-30, about 440 trading days. The end date is fixed now.
- **Source:** Tiingo daily prices adjusted for splits and dividends, as in Phases 3–6. The data is downloaded once, after this document is committed, into `data/fresh/`, starting 2023-06-01 so every strategy has a year of history before the window opens.
- **Universes:**
  - **Six:** AAPL, JPM, KO, MSFT, SPY, XOM.
  - **Dow 29:** the Dow Jones Industrial Average of 1 January 2014, without DD (as in [Phase 6](phase-6-robustness.md#data-notes)).
- **No exclusions after the fact.** The test program refuses a data folder that doesn't hold exactly these symbols. If any symbol can't be downloaded or doesn't cover the window, that universe's test is reported as not run. No substitutes, no dropped symbols.

## Strategies

| Strategy | Rule | Parameter grid |
|---|---|---|
| Mean reversion | Buy when the close is z standard deviations below its trailing average; sell when it is back above (Phases 3–6) | 32 sets: lookback 10/20/30/50, entry z 1/1.5/2/2.5, exit z 0/0.5 |
| MA crossover | Hold while the short moving average is above the long one (Phase 4) | 8 sets: short 10/20/50, long 50/100/200 |
| Mean reversion + trend filter | Mean reversion, but only buy while the close is above its 200-day average | Same 32 sets |
| Mean reversion + time stop | Mean reversion, but sell any position held 21 trading days (about a month) | Same 32 sets |
| Buy-and-hold | Benchmark | — |

The two refinements come from earlier findings on 2014–2024 data. Phase 3's worst trades were dips that kept falling, which the trend filter targets. Its trades held over a month mostly lost, which the time stop targets. The 200-day and 21-day settings are fixed, not tuned.

**Same engine settings as before:** $100,000 per symbol, all available cash in each trade, 0.1% commission and 5 bps slippage per side, fills at the next day's open, no interest on cash (Sharpe uses a zero risk-free rate), 252 days of warm-up history.

## Chosen parameters

Each strategy's parameters were chosen by the rule the Phase 6 walk-forward used: **the highest average Sharpe ratio across the universe's symbols on the three years before the test (2022–2024)**. Full rankings are in [`results/phase7/six/selection.md`](../results/phase7/six/selection.md) and [`results/phase7/dow29/selection.md`](../results/phase7/dow29/selection.md).

| Strategy | Six symbols | Dow 29 |
|---|---|---|
| Mean reversion | lookback 30, entry z 2.5, exit z 0 | lookback 50, entry z 1, exit z 0.5 |
| MA crossover | 20 / 200 | 20 / 200 |
| Mean reversion + trend filter | lookback 30, entry z 2, exit z 0 | lookback 30, entry z 2.5, exit z 0.5 |
| Mean reversion + time stop | lookback 30, entry z 2, exit z 0 | lookback 50, entry z 1, exit z 0.5 |

Anyone can check these by running `fresh_data_test select` on the 2014–2024 data: it reruns the selection and reports whether each recorded choice matches.

## Hypotheses

The measure is the **average across symbols of each strategy's annualized Sharpe ratio** on the test window. Each hypothesis is tested separately on each universe.

| | Hypothesis | Comparison | Based on |
|---|---|---|---|
| H1 | Mean reversion trails buy-and-hold | Mean reversion minus buy-and-hold < 0 | Phases 3 and 6 |
| H2 | MA crossover trails buy-and-hold | MA crossover minus buy-and-hold < 0 | Phase 6 |
| H3 | The trend filter improves mean reversion | Trend-filtered minus plain mean reversion > 0 | Phase 3's worst trades |
| H4 | The time stop improves mean reversion | Time-stopped minus plain mean reversion > 0 | Phase 3's holding-period pattern |

**How each is judged.** Resample the test window's daily returns 1,000 times in 21-day blocks (the same days for every symbol and strategy; seed 7, as in Phase 6) and count how often the predicted side comes out ahead.

- **Supported:** the predicted side wins in at least 95% of resamples.
- **Contradicted:** the other side wins in at least 95%.
- **Inconclusive:** anything in between.

**What to expect.** Twenty-one months is short. Statistical uncertainty in a Sharpe ratio shrinks with the square root of time, so these ranges will be about 1.7 times wider than Phase 6's five-year ones. Inconclusive verdicts are likely, especially for H3 and H4, where the strategies differ only on some trades. An inconclusive result will be reported as inconclusive, not as weak support.

## What will be reported

Everything the test program prints, for both universes:
- the average results;
- the H1–H4 verdicts with their gaps and ranges;
- every other pairwise comparison, labeled exploratory;
- Sharpe ratios by symbol;
- a growth chart.

Anything analyzed after seeing the test data will be labeled exploratory and kept apart from H1–H4.

## Commitments

1. **The test runs once per universe.** Running it again only reproduces the same numbers.
2. **No changes after the download.** Once the test data exists, code changes are limited to fixing a bug that stops the program from running. Any such change will be listed in the results write-up with its diff.
3. **The parameters in `apps/preregistration.h` don't change.**
4. **The results are published whatever they show.**

## Reproducing the selection

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
build/apps/fresh_data_test select data/market six > results/phase7/six/selection.md
build/apps/fresh_data_test select data/dow30 dow29 > results/phase7/dow29/selection.md
```

After this document is committed, the test data is downloaded and the test is run:

```bash
scripts/fetch_market_data.sh --out data/fresh/market --from 2023-06-01 --to 2026-09-30
scripts/fetch_market_data.sh --out data/fresh/dow30 --from 2023-06-01 --to 2026-09-30 \
    $(grep -v '^#' scripts/universes/dow30_2014.txt)
build/apps/fresh_data_test run data/fresh/market results/phase7/six six > results/phase7/six/test_output.md
build/apps/fresh_data_test run data/fresh/dow30 results/phase7/dow29 dow29 > results/phase7/dow29/test_output.md
```
