# Phase 7: pre-registered test, dow29

29 symbols, 2025-01-01 to 2026-09-30, run once with the parameters recorded before the data was downloaded. Costs: 0.1% commission + 5 bps slippage per side. Fills at next open. No interest on cash.

## Results, average of 29 symbols

| Strategy | Parameters | CAGR | Sharpe | Max DD | Trades | Win rate | In market |
|---|---|---:|---:|---:|---:|---:|---:|
| Mean reversion | lookback=50 entry_z=1 exit_z=0.5 | 7.5% | 0.52 | 18.9% | 5.72 | 71.4% | 36.5% |
| MA crossover | short_window=20 long_window=200 | 12.3% | 0.33 | 23.1% | 2.59 | 46.0% | 69.6% |
| Mean reversion + trend filter | lookback=30 entry_z=2.5 exit_z=0.5 | 1.5% | 0.21 | 6.6% | 1.38 | 45.4% | 7.1% |
| Mean reversion + time stop | lookback=50 entry_z=1 exit_z=0.5 | 6.3% | 0.46 | 18.4% | 8.79 | 60.2% | 32.8% |
| Buy-and-hold |  | 20.8% | 0.66 | 26.6% | 1.00 | 79.3% | 99.8% |

## Pre-registered hypotheses

Average Sharpe gap and its 95% range from 1000 block-bootstrap resamples (21-day blocks). A hypothesis is supported if the predicted side wins in at least 95.0% of resamples, contradicted if it loses in at least 95.0%, and otherwise inconclusive.

| | Hypothesis | Comparison | Sharpe gap | 95% range | First ahead in | Verdict |
|---|---|---|---:|---:|---:|---|
| H1 | Mean reversion trails buy-and-hold | Mean reversion minus Buy-and-hold | -0.15 | -0.41 to 0.21 | 22.8% | Inconclusive |
| H2 | MA crossover trails buy-and-hold | MA crossover minus Buy-and-hold | -0.33 | -0.57 to -0.15 | 0.2% | Supported |
| H3 | The trend filter improves mean reversion | Mean reversion + trend filter minus Mean reversion | -0.31 | -0.72 to 0.02 | 2.9% | Contradicted |
| H4 | The time stop improves mean reversion | Mean reversion + time stop minus Mean reversion | -0.06 | -0.15 to 0.01 | 5.6% | Inconclusive |

### All comparisons (exploratory beyond H1-H4)

1000 resamples of 436 daily returns in 21-day blocks; average Sharpe across symbols.

| Comparison | Sharpe gap | 95% range | First ahead in |
|---|---:|---:|---:|
| Mean reversion minus Buy-and-hold | -0.15 | -0.41 to 0.21 | 22.8% |
| MA crossover minus Buy-and-hold | -0.33 | -0.57 to -0.15 | 0.2% |
| Mean reversion + trend filter minus Buy-and-hold | -0.45 | -0.95 to 0.02 | 2.8% |
| Mean reversion + time stop minus Buy-and-hold | -0.21 | -0.46 to 0.14 | 11.5% |
| MA crossover minus Mean reversion | -0.19 | -0.56 to 0.11 | 8.7% |
| Mean reversion + trend filter minus Mean reversion | -0.31 | -0.72 to 0.02 | 2.9% |
| Mean reversion + time stop minus Mean reversion | -0.06 | -0.15 to 0.01 | 5.6% |
| Mean reversion + trend filter minus MA crossover | -0.12 | -0.53 to 0.26 | 27.5% |
| Mean reversion + time stop minus MA crossover | 0.13 | -0.13 to 0.46 | 86.2% |
| Mean reversion + time stop minus Mean reversion + trend filter | 0.25 | -0.05 to 0.64 | 95.8% |

## Sharpe ratio by symbol

| Symbol | MR | MA | MRT | MRS | B&H |
|---|---:|---:|---:|---:|---:|
| AXP | -0.15 | -0.29 | 0.00 | -0.03 | 0.20 |
| BA | 0.96 | -0.76 | 0.17 | 0.84 | 0.31 |
| CAT | 0.11 | 1.47 | 0.00 | 0.07 | 1.50 |
| CSCO | 1.12 | 1.35 | 0.29 | 1.14 | 1.35 |
| CVX | 1.26 | 0.61 | 0.90 | 0.96 | 1.04 |
| DIS | 0.27 | -1.09 | 0.12 | 0.27 | 0.05 |
| GE | 0.27 | 1.22 | 0.48 | 0.53 | 1.26 |
| GS | -0.14 | 0.66 | -0.39 | 0.17 | 1.00 |
| HD | -0.09 | -1.47 | 0.00 | -0.29 | -0.52 |
| IBM | 0.91 | -0.23 | 0.56 | 0.91 | 0.27 |
| INTC | 1.08 | 1.57 | 0.66 | 1.17 | 1.75 |
| JNJ | 0.83 | 1.82 | -0.03 | 1.28 | 2.00 |
| JPM | 0.87 | 0.48 | 0.04 | 0.65 | 0.94 |
| KO | 1.32 | 0.81 | 0.00 | 1.29 | 1.27 |
| MCD | -0.62 | 0.06 | -0.50 | -0.64 | -0.50 |
| MMM | 0.99 | 0.33 | 0.71 | 0.87 | 0.68 |
| MRK | -0.09 | 1.62 | 0.00 | -0.47 | 0.98 |
| MSFT | -0.05 | 0.13 | 0.16 | -0.60 | 0.55 |
| NKE | -0.22 | -0.58 | 0.00 | -0.17 | -0.82 |
| PFE | -0.03 | 0.20 | 0.00 | -0.17 | 0.56 |
| PG | 0.77 | -1.35 | 0.00 | 0.77 | -0.17 |
| T | 0.03 | 0.15 | -0.36 | -0.35 | 0.45 |
| TRV | 1.37 | 1.18 | 2.02 | 1.33 | 1.18 |
| UNH | -0.21 | -0.49 | -0.84 | -0.04 | -0.15 |
| UTX | 0.84 | 1.08 | -0.06 | 0.64 | 1.20 |
| V | 0.80 | 0.13 | 1.42 | 0.58 | 0.48 |
| VZ | 1.28 | -0.31 | 0.86 | 1.18 | 0.71 |
| WMT | 0.12 | 0.65 | -0.13 | -0.08 | 0.47 |
| XOM | 1.37 | 0.56 | 0.00 | 1.44 | 1.17 |

Results written to results/phase7/dow29/
