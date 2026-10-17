# Phase 7: pre-registered test, six

6 symbols, 2025-01-01 to 2026-09-30, run once with the parameters recorded before the data was downloaded. Costs: 0.1% commission + 5 bps slippage per side. Fills at next open. No interest on cash.

## Results, average of 6 symbols

| Strategy | Parameters | CAGR | Sharpe | Max DD | Trades | Win rate | In market |
|---|---|---:|---:|---:|---:|---:|---:|
| Mean reversion | lookback=30 entry_z=2.5 exit_z=0 | 2.1% | 0.32 | 12.4% | 2.50 | 61.1% | 10.0% |
| MA crossover | short_window=20 long_window=200 | 10.3% | 0.63 | 16.6% | 2.67 | 55.6% | 77.1% |
| Mean reversion + trend filter | lookback=30 entry_z=2 exit_z=0 | 2.9% | 0.42 | 8.8% | 3.17 | 72.6% | 9.8% |
| Mean reversion + time stop | lookback=30 entry_z=2 exit_z=0 | 4.4% | 0.53 | 12.2% | 5.67 | 80.9% | 16.5% |
| Buy-and-hold |  | 21.1% | 0.95 | 22.9% | 1.00 | 100.0% | 99.8% |

## Pre-registered hypotheses

Average Sharpe gap and its 95% range from 1000 block-bootstrap resamples (21-day blocks). A hypothesis is supported if the predicted side wins in at least 95.0% of resamples, contradicted if it loses in at least 95.0%, and otherwise inconclusive.

| | Hypothesis | Comparison | Sharpe gap | 95% range | First ahead in | Verdict |
|---|---|---|---:|---:|---:|---|
| H1 | Mean reversion trails buy-and-hold | Mean reversion minus Buy-and-hold | -0.64 | -1.31 to -0.09 | 1.1% | Supported |
| H2 | MA crossover trails buy-and-hold | MA crossover minus Buy-and-hold | -0.32 | -0.80 to 0.07 | 5.6% | Inconclusive |
| H3 | The trend filter improves mean reversion | Mean reversion + trend filter minus Mean reversion | 0.11 | -0.32 to 0.64 | 70.1% | Inconclusive |
| H4 | The time stop improves mean reversion | Mean reversion + time stop minus Mean reversion | 0.22 | -0.07 to 0.65 | 91.4% | Inconclusive |

### All comparisons (exploratory beyond H1-H4)

1000 resamples of 436 daily returns in 21-day blocks; average Sharpe across symbols.

| Comparison | Sharpe gap | 95% range | First ahead in |
|---|---:|---:|---:|
| Mean reversion minus Buy-and-hold | -0.64 | -1.31 to -0.09 | 1.1% |
| MA crossover minus Buy-and-hold | -0.32 | -0.80 to 0.07 | 5.6% |
| Mean reversion + trend filter minus Buy-and-hold | -0.53 | -1.08 to -0.03 | 1.8% |
| Mean reversion + time stop minus Buy-and-hold | -0.42 | -0.95 to 0.08 | 5.5% |
| MA crossover minus Mean reversion | 0.31 | -0.33 to 1.04 | 82.1% |
| Mean reversion + trend filter minus Mean reversion | 0.11 | -0.32 to 0.64 | 70.1% |
| Mean reversion + time stop minus Mean reversion | 0.22 | -0.07 to 0.65 | 91.4% |
| Mean reversion + trend filter minus MA crossover | -0.21 | -0.76 to 0.33 | 26.8% |
| Mean reversion + time stop minus MA crossover | -0.10 | -0.73 to 0.54 | 41.9% |
| Mean reversion + time stop minus Mean reversion + trend filter | 0.11 | -0.21 to 0.44 | 73.8% |

## Sharpe ratio by symbol

| Symbol | MR | MA | MRT | MRS | B&H |
|---|---:|---:|---:|---:|---:|
| AAPL | 0.54 | 0.85 | 0.87 | 0.03 | 0.76 |
| JPM | 0.05 | 0.48 | 0.21 | 0.72 | 0.94 |
| KO | 1.22 | 0.81 | 0.98 | 1.23 | 1.27 |
| MSFT | 0.23 | 0.13 | -0.23 | 0.10 | 0.55 |
| SPY | -0.15 | 0.93 | 0.31 | 0.60 | 1.03 |
| XOM | 0.00 | 0.56 | 0.39 | 0.50 | 1.17 |

Results written to results/phase7/six/
