# Phase 7: pre-registered test, six

6 symbols, 2025-01-01 to 2026-09-30, run once with the parameters recorded before the data was downloaded. Costs: 0.1% commission + 5 bps slippage per side. Fills at next open. No interest on cash.

## Results, average of 6 symbols

| Strategy | Parameters | CAGR | Sharpe | Max DD | Trades | Win rate | In market |
|---|---|---:|---:|---:|---:|---:|---:|
| Mean reversion | lookback=30 entry_z=2.5 exit_z=0 | 2.0% | 0.24 | 10.2% | 3.00 | 48.6% | 13.6% |
| MA crossover | short_window=20 long_window=200 | 1.0% | -0.07 | 22.7% | 3.33 | 47.2% | 53.8% |
| Mean reversion + trend filter | lookback=30 entry_z=2 exit_z=0 | 1.1% | 0.22 | 6.3% | 1.67 | 55.6% | 6.1% |
| Mean reversion + time stop | lookback=30 entry_z=2 exit_z=0 | -0.6% | 0.02 | 17.1% | 5.83 | 54.4% | 19.7% |
| Buy-and-hold |  | 7.7% | 0.41 | 29.1% | 1.00 | 66.7% | 99.8% |

## Pre-registered hypotheses

Average Sharpe gap and its 95% range from 1000 block-bootstrap resamples (21-day blocks). A hypothesis is supported if the predicted side wins in at least 95.0% of resamples, contradicted if it loses in at least 95.0%, and otherwise inconclusive.

| | Hypothesis | Comparison | Sharpe gap | 95% range | First ahead in | Verdict |
|---|---|---|---:|---:|---:|---|
| H1 | Mean reversion trails buy-and-hold | Mean reversion minus Buy-and-hold | -0.17 | -0.74 to 0.42 | 34.3% | Inconclusive |
| H2 | MA crossover trails buy-and-hold | MA crossover minus Buy-and-hold | -0.48 | -0.89 to -0.17 | 0.2% | Supported |
| H3 | The trend filter improves mean reversion | Mean reversion + trend filter minus Mean reversion | -0.02 | -0.47 to 0.41 | 42.6% | Inconclusive |
| H4 | The time stop improves mean reversion | Mean reversion + time stop minus Mean reversion | -0.22 | -0.67 to 0.24 | 16.8% | Inconclusive |

### All comparisons (exploratory beyond H1-H4)

1000 resamples of 455 daily returns in 21-day blocks; average Sharpe across symbols.

| Comparison | Sharpe gap | 95% range | First ahead in |
|---|---:|---:|---:|
| Mean reversion minus Buy-and-hold | -0.17 | -0.74 to 0.42 | 34.3% |
| MA crossover minus Buy-and-hold | -0.48 | -0.89 to -0.17 | 0.2% |
| Mean reversion + trend filter minus Buy-and-hold | -0.19 | -0.79 to 0.41 | 26.0% |
| Mean reversion + time stop minus Buy-and-hold | -0.39 | -0.85 to 0.11 | 8.7% |
| MA crossover minus Mean reversion | -0.31 | -0.96 to 0.21 | 9.2% |
| Mean reversion + trend filter minus Mean reversion | -0.02 | -0.47 to 0.41 | 42.6% |
| Mean reversion + time stop minus Mean reversion | -0.22 | -0.67 to 0.24 | 16.8% |
| Mean reversion + trend filter minus MA crossover | 0.29 | -0.18 to 0.87 | 91.2% |
| Mean reversion + time stop minus MA crossover | 0.09 | -0.42 to 0.69 | 76.2% |
| Mean reversion + time stop minus Mean reversion + trend filter | -0.20 | -0.68 to 0.30 | 24.6% |

## Sharpe ratio by symbol

| Symbol | MR | MA | MRT | MRS | B&H |
|---|---:|---:|---:|---:|---:|
| AAPL | 0.12 | 0.20 | 0.99 | -0.09 | 0.44 |
| JPM | 0.20 | -0.89 | 0.00 | 0.42 | -0.08 |
| KO | 1.07 | 0.78 | 0.37 | 1.16 | 0.96 |
| MSFT | 0.55 | -0.24 | -0.00 | -0.07 | 0.72 |
| SPY | -0.37 | 0.68 | -0.33 | -1.12 | 0.69 |
| XOM | -0.13 | -0.95 | 0.28 | -0.20 | -0.28 |

Results written to fresh_data_test/
