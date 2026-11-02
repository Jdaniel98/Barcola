# Strategy study

Symbols: 6. Costs: 0.1% commission + 5 bps slippage per side. Fills at next open. No interest on cash.

## MeanReversion

Chosen in-sample (2014-01-01 to 2019-12-31): lookback=20 entry_z=2.5 exit_z=0, mean Sharpe 0.31 (best of 32).
Out-of-sample it ranks 28 of 32 parameter sets by mean Sharpe (0.11; best 0.38, median 0.18).
Out-of-sample, 32 of 32 parameter sets have a positive mean Sharpe and 0 beat buy-and-hold's mean Sharpe of 0.49.

### In-sample 2014-01-01 to 2019-12-31

| Symbol | MR CAGR | MR Sharpe | MR max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | 3.1% | 0.43 | 17.9% | 13 | 76.9% | 11.6% | -3.7% | -0.04 | 58.3% |
| JPM | 5.7% | 0.97 | 6.0% | 10 | 90.0% | 6.1% | 11.3% | 0.57 | 37.3% |
| KO | -1.5% | -0.12 | 18.1% | 16 | 68.8% | 16.9% | -14.5% | -0.58 | 65.6% |
| MSFT | 2.5% | 0.44 | 15.8% | 8 | 75.0% | 7.2% | 20.7% | 0.90 | 26.8% |
| SPY | -2.6% | -0.22 | 28.6% | 15 | 33.3% | 17.8% | 4.5% | 0.31 | 49.1% |
| XOM | 2.3% | 0.36 | 11.6% | 9 | 66.7% | 8.2% | 7.4% | 0.42 | 41.6% |
| **Average** | 1.6% | 0.31 | 16.3% | 11.83 | 68.4% | 11.3% | 4.3% | 0.26 | 46.4% |

### Out-of-sample 2020-01-01 to 2024-12-31

| Symbol | MR CAGR | MR Sharpe | MR max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | 3.5% | 0.57 | 10.2% | 8 | 87.5% | 7.7% | 7.0% | 0.41 | 31.4% |
| JPM | 3.4% | 0.51 | 9.9% | 9 | 77.8% | 8.4% | 8.0% | 0.44 | 31.3% |
| KO | 3.8% | 0.75 | 9.0% | 6 | 83.3% | 5.7% | 20.1% | 0.89 | 21.5% |
| MSFT | -1.0% | -0.14 | 16.0% | 8 | 50.0% | 10.3% | -5.5% | -0.15 | 37.7% |
| SPY | -3.8% | -0.50 | 27.0% | 7 | 42.9% | 11.0% | 6.5% | 0.39 | 33.1% |
| XOM | -4.1% | -0.55 | 25.0% | 7 | 42.9% | 13.2% | 21.6% | 0.94 | 40.9% |
| **Average** | 0.3% | 0.11 | 16.2% | 7.50 | 64.1% | 9.4% | 9.6% | 0.49 | 32.7% |

### Trades by holding period, in-sample

| Held | Trades | Win rate | Average return |
|---|---:|---:|---:|
| 0-14 days | 27 | 100.0% | 4.1% |
| 15-30 days | 29 | 62.1% | 1.8% |
| 31-60 days | 15 | 13.3% | -5.6% |

### Trades by holding period, out-of-sample

| Held | Trades | Win rate | Average return |
|---|---:|---:|---:|
| 0-14 days | 20 | 100.0% | 3.7% |
| 15-30 days | 17 | 58.8% | 0.8% |
| 31-60 days | 5 | 0.0% | -4.7% |
| 61+ days | 3 | 0.0% | -14.2% |

## MACrossover

Chosen in-sample (2014-01-01 to 2019-12-31): short_window=20 long_window=100, mean Sharpe 0.26 (best of 8).
Out-of-sample it ranks 3 of 8 parameter sets by mean Sharpe (0.29; best 0.37, median 0.20).
Out-of-sample, 8 of 8 parameter sets have a positive mean Sharpe and 0 beat buy-and-hold's mean Sharpe of 0.49.

### In-sample 2014-01-01 to 2019-12-31

| Symbol | MA CAGR | MA Sharpe | MA max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | -5.6% | -0.30 | 46.2% | 13 | 38.5% | 46.0% | -3.7% | -0.04 | 58.3% |
| JPM | 7.3% | 0.51 | 27.6% | 10 | 40.0% | 51.6% | 11.3% | 0.57 | 37.3% |
| KO | -5.2% | -0.34 | 34.0% | 12 | 33.3% | 34.1% | -14.5% | -0.58 | 65.6% |
| MSFT | 21.3% | 1.06 | 19.5% | 8 | 87.5% | 65.0% | 20.7% | 0.90 | 26.8% |
| SPY | 5.4% | 0.41 | 33.6% | 9 | 66.7% | 52.7% | 4.5% | 0.31 | 49.1% |
| XOM | 2.9% | 0.25 | 25.5% | 12 | 25.0% | 52.3% | 7.4% | 0.42 | 41.6% |
| **Average** | 4.4% | 0.26 | 31.1% | 10.67 | 48.5% | 50.3% | 4.3% | 0.26 | 46.4% |

### Out-of-sample 2020-01-01 to 2024-12-31

| Symbol | MA CAGR | MA Sharpe | MA max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | -0.7% | 0.04 | 33.6% | 11 | 27.3% | 53.3% | 7.0% | 0.41 | 31.4% |
| JPM | 3.3% | 0.26 | 32.0% | 11 | 45.5% | 55.7% | 8.0% | 0.44 | 31.3% |
| KO | 8.6% | 0.52 | 17.6% | 10 | 50.0% | 65.4% | 20.1% | 0.89 | 21.5% |
| MSFT | -0.4% | 0.05 | 25.0% | 10 | 40.0% | 46.0% | -5.5% | -0.15 | 37.7% |
| SPY | -4.6% | -0.18 | 38.2% | 10 | 40.0% | 58.0% | 6.5% | 0.39 | 33.1% |
| XOM | 20.2% | 1.04 | 21.5% | 6 | 83.3% | 68.4% | 21.6% | 0.94 | 40.9% |
| **Average** | 4.4% | 0.29 | 28.0% | 9.67 | 47.7% | 57.8% | 9.6% | 0.49 | 32.7% |

### Trades by holding period, in-sample

| Held | Trades | Win rate | Average return |
|---|---:|---:|---:|
| 0-14 days | 9 | 11.1% | -2.7% |
| 15-30 days | 2 | 0.0% | -5.8% |
| 31-60 days | 15 | 6.7% | -7.7% |
| 61+ days | 38 | 71.1% | 9.7% |

### Trades by holding period, out-of-sample

| Held | Trades | Win rate | Average return |
|---|---:|---:|---:|
| 0-14 days | 5 | 20.0% | -2.2% |
| 15-30 days | 8 | 0.0% | -6.3% |
| 31-60 days | 3 | 0.0% | -4.2% |
| 61+ days | 42 | 64.3% | 5.9% |

## Comparison

### In-sample 2014-01-01 to 2019-12-31, average of 6 symbols

| Strategy | CAGR | Sharpe | Max DD | Trades | Win rate | In market |
|---|---:|---:|---:|---:|---:|---:|
| MeanReversion (lookback=20 entry_z=2.5 exit_z=0) | 1.6% | 0.31 | 16.3% | 11.83 | 68.4% | 11.3% |
| MACrossover (short_window=20 long_window=100) | 4.4% | 0.26 | 31.1% | 10.67 | 48.5% | 50.3% |
| BuyAndHold | 4.3% | 0.26 | 46.4% | 1 | - | 99.9% |

### Out-of-sample 2020-01-01 to 2024-12-31, average of 6 symbols

| Strategy | CAGR | Sharpe | Max DD | Trades | Win rate | In market |
|---|---:|---:|---:|---:|---:|---:|
| MeanReversion (lookback=20 entry_z=2.5 exit_z=0) | 0.3% | 0.11 | 16.2% | 7.50 | 64.1% | 9.4% |
| MACrossover (short_window=20 long_window=100) | 4.4% | 0.29 | 28.0% | 9.67 | 47.7% | 57.8% |
| BuyAndHold | 9.6% | 0.49 | 32.7% | 1 | - | 99.9% |

### Correlation of daily returns, MR vs MA, out-of-sample

| Symbol | Correlation |
|---|---:|
| AAPL | 0.11 |
| JPM | 0.09 |
| KO | 0.09 |
| MSFT | 0.09 |
| SPY | 0.16 |
| XOM | 0.14 |
| **Average** | 0.11 |

## Robustness

### Confidence intervals, 2020 to 2024

1000 resamples of 1304 daily returns in 21-day blocks; average Sharpe across symbols.

| Comparison | Sharpe gap | 95% range | First ahead in |
|---|---:|---:|---:|
| Mean reversion minus Buy-and-hold | -0.38 | -0.78 to -0.04 | 1.3% |
| MA crossover minus Buy-and-hold | -0.20 | -0.45 to 0.04 | 6.0% |
| MA crossover minus Mean reversion | 0.18 | -0.18 to 0.64 | 82.9% |

### Walk-forward, 2016 to 2024

Each year's parameters are chosen on the 3 years before it, by average Sharpe across symbols, then run on that year alone.

| Year | MR chosen | MR Sharpe | MA chosen | MA Sharpe | B&H Sharpe |
|---|---|---:|---|---:|---:|
| 2016 | lookback=50 entry_z=2 exit_z=0.5 | 0.34 | short_window=20 long_window=100 | 0.14 | 0.06 |
| 2017 | lookback=20 entry_z=2.5 exit_z=0 | 0.62 | short_window=10 long_window=50 | 0.40 | 0.42 |
| 2018 | lookback=20 entry_z=2.5 exit_z=0 | 0.17 | short_window=10 long_window=50 | 0.03 | 0.55 |
| 2019 | lookback=20 entry_z=2.5 exit_z=0.5 | 0.09 | short_window=20 long_window=100 | -0.27 | -0.09 |
| 2020 | lookback=20 entry_z=2.5 exit_z=0 | 0.40 | short_window=10 long_window=100 | 0.07 | 0.52 |
| 2021 | lookback=50 entry_z=2 exit_z=0 | 0.47 | short_window=50 long_window=200 | 0.54 | 0.86 |
| 2022 | lookback=20 entry_z=2.5 exit_z=0.5 | 0.18 | short_window=50 long_window=200 | -0.02 | 0.42 |
| 2023 | lookback=20 entry_z=2.5 exit_z=0.5 | 0.80 | short_window=10 long_window=50 | -0.21 | 0.57 |
| 2024 | lookback=10 entry_z=1.5 exit_z=0.5 | -0.05 | short_window=10 long_window=50 | 0.16 | 0.06 |

| Strategy, all test years compounded | CAGR | Sharpe | Max DD | Years ahead of B&H |
|---|---:|---:|---:|---:|
| Mean reversion | 1.4% | 0.18 | 23.3% | 4 of 9 |
| MA crossover | 2.3% | 0.21 | 39.3% | 2 of 9 |
| Buy-and-hold | 6.2% | 0.37 | 46.4% | - |

### Confidence intervals, walk-forward 2016 to 2024

1000 resamples of 2339 daily returns in 21-day blocks; average Sharpe across symbols.

| Comparison | Sharpe gap | 95% range | First ahead in |
|---|---:|---:|---:|
| Mean reversion minus Buy-and-hold | -0.19 | -0.45 to 0.10 | 9.8% |
| MA crossover minus Buy-and-hold | -0.16 | -0.35 to 0.02 | 4.1% |
| MA crossover minus Mean reversion | 0.02 | -0.30 to 0.31 | 52.7% |

### Cost sensitivity, 2020 to 2024

The chosen parameters at multiples of the study's costs (1x = 0.1% commission + 5 bps slippage per side). Average of 6 symbols.

| Costs | MR CAGR | MR Sharpe | MA CAGR | MA Sharpe | B&H CAGR | B&H Sharpe |
|---|---:|---:|---:|---:|---:|---:|
| 0x | 0.7% | 0.17 | 5.0% | 0.32 | 9.7% | 0.49 |
| 1x | 0.3% | 0.11 | 4.4% | 0.29 | 9.6% | 0.49 |
| 2x | -0.1% | 0.04 | 3.8% | 0.26 | 9.6% | 0.48 |
| 3x | -0.6% | -0.03 | 3.2% | 0.22 | 9.5% | 0.48 |

CSV results written to strategy_study/
