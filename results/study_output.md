# Strategy study

Symbols: 6. Costs: 0.1% commission + 5 bps slippage per side. Fills at next open. No interest on cash.

## MeanReversion

Chosen in-sample (2014-01-01 to 2019-12-31): lookback=20 entry_z=2 exit_z=0.5, mean Sharpe 0.75 (best of 32).
Out-of-sample it ranks 7 of 32 parameter sets by mean Sharpe (0.30; best 0.35, median 0.25).
Out-of-sample, 31 of 32 parameter sets have a positive mean Sharpe and 0 beat buy-and-hold's mean Sharpe of 0.67.

### In-sample 2014-01-01 to 2019-12-31

| Symbol | MR CAGR | MR Sharpe | MR max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | 3.9% | 0.33 | 32.0% | 21 | 81.0% | 22.1% | 26.7% | 1.09 | 38.5% |
| JPM | 11.2% | 0.93 | 14.1% | 30 | 80.0% | 23.6% | 18.6% | 0.93 | 23.2% |
| KO | 9.1% | 1.22 | 7.0% | 30 | 86.7% | 26.2% | 8.7% | 0.65 | 13.9% |
| MSFT | 13.9% | 1.14 | 7.8% | 21 | 95.2% | 15.9% | 30.1% | 1.28 | 18.2% |
| SPY | 7.5% | 0.85 | 8.9% | 27 | 85.2% | 20.7% | 12.0% | 0.93 | 19.3% |
| XOM | -0.7% | 0.01 | 22.6% | 27 | 55.6% | 31.7% | -2.3% | -0.03 | 31.5% |
| **Average** | 7.5% | 0.75 | 15.4% | 26.00 | 80.6% | 23.3% | 15.6% | 0.81 | 24.1% |

### Out-of-sample 2020-01-01 to 2024-12-31

| Symbol | MR CAGR | MR Sharpe | MR max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | 4.8% | 0.33 | 25.9% | 18 | 61.1% | 22.3% | 28.3% | 0.95 | 31.4% |
| JPM | 11.9% | 0.61 | 37.8% | 22 | 68.2% | 25.5% | 14.9% | 0.59 | 43.1% |
| KO | -1.3% | -0.01 | 35.7% | 16 | 50.0% | 26.3% | 5.9% | 0.38 | 37.0% |
| MSFT | 11.1% | 0.62 | 21.6% | 23 | 69.6% | 22.3% | 22.7% | 0.82 | 37.1% |
| SPY | 3.2% | 0.28 | 28.6% | 20 | 75.0% | 21.3% | 14.5% | 0.75 | 33.7% |
| XOM | -2.7% | -0.02 | 52.8% | 20 | 65.0% | 28.7% | 13.9% | 0.55 | 55.3% |
| **Average** | 4.5% | 0.30 | 33.7% | 19.83 | 64.8% | 24.4% | 16.7% | 0.67 | 39.6% |

### Trades by holding period, in-sample

| Held | Trades | Win rate | Average return |
|---|---:|---:|---:|
| 0-14 days | 64 | 100.0% | 3.6% |
| 15-30 days | 72 | 79.2% | 1.9% |
| 31-60 days | 18 | 33.3% | -1.7% |
| 61+ days | 2 | 0.0% | -17.0% |

### Trades by holding period, out-of-sample

| Held | Trades | Win rate | Average return |
|---|---:|---:|---:|
| 0-14 days | 49 | 93.9% | 5.6% |
| 15-30 days | 42 | 71.4% | 2.1% |
| 31-60 days | 26 | 7.7% | -5.1% |
| 61+ days | 2 | 0.0% | -21.4% |

## MACrossover

Chosen in-sample (2014-01-01 to 2019-12-31): short_window=20 long_window=200, mean Sharpe 0.59 (best of 8).
Out-of-sample it ranks 2 of 8 parameter sets by mean Sharpe (0.65; best 0.68, median 0.52).
Out-of-sample, 8 of 8 parameter sets have a positive mean Sharpe and 1 beat buy-and-hold's mean Sharpe of 0.67.

### In-sample 2014-01-01 to 2019-12-31

| Symbol | MA CAGR | MA Sharpe | MA max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | 22.6% | 1.13 | 33.6% | 4 | 75.0% | 76.6% | 26.7% | 1.09 | 38.5% |
| JPM | 9.0% | 0.58 | 26.9% | 7 | 42.9% | 81.7% | 18.6% | 0.93 | 23.2% |
| KO | 3.8% | 0.37 | 17.5% | 7 | 71.4% | 70.5% | 8.7% | 0.65 | 13.9% |
| MSFT | 22.9% | 1.07 | 23.4% | 5 | 80.0% | 92.2% | 30.1% | 1.28 | 18.2% |
| SPY | 9.3% | 0.85 | 14.5% | 4 | 75.0% | 88.3% | 12.0% | 0.93 | 19.3% |
| XOM | -5.4% | -0.42 | 32.1% | 8 | 12.5% | 48.1% | -2.3% | -0.03 | 31.5% |
| **Average** | 10.4% | 0.59 | 24.7% | 5.83 | 59.5% | 76.2% | 15.6% | 0.81 | 24.1% |

### Out-of-sample 2020-01-01 to 2024-12-31

| Symbol | MA CAGR | MA Sharpe | MA max DD | Trades | Win rate | In market | B&H CAGR | B&H Sharpe | B&H max DD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AAPL | 18.3% | 0.74 | 34.7% | 5 | 60.0% | 82.0% | 28.3% | 0.95 | 31.4% |
| JPM | 14.2% | 0.71 | 36.6% | 3 | 66.7% | 71.1% | 14.9% | 0.59 | 43.1% |
| KO | -0.8% | 0.03 | 33.0% | 5 | 40.0% | 73.9% | 5.9% | 0.38 | 37.0% |
| MSFT | 22.0% | 0.89 | 28.0% | 6 | 33.3% | 79.5% | 22.7% | 0.82 | 37.1% |
| SPY | 8.6% | 0.60 | 32.2% | 4 | 50.0% | 77.9% | 14.5% | 0.75 | 33.7% |
| XOM | 21.4% | 0.92 | 20.5% | 4 | 25.0% | 69.6% | 13.9% | 0.55 | 55.3% |
| **Average** | 13.9% | 0.65 | 30.8% | 4.50 | 45.8% | 75.7% | 16.7% | 0.67 | 39.6% |

### Trades by holding period, in-sample

| Held | Trades | Win rate | Average return |
|---|---:|---:|---:|
| 15-30 days | 3 | 0.0% | -6.6% |
| 31-60 days | 3 | 0.0% | -8.0% |
| 61+ days | 29 | 65.5% | 17.5% |

### Trades by holding period, out-of-sample

| Held | Trades | Win rate | Average return |
|---|---:|---:|---:|
| 0-14 days | 4 | 0.0% | -2.4% |
| 15-30 days | 2 | 0.0% | -4.0% |
| 31-60 days | 1 | 0.0% | -13.0% |
| 61+ days | 20 | 60.0% | 32.0% |

## Comparison

### In-sample 2014-01-01 to 2019-12-31, average of 6 symbols

| Strategy | CAGR | Sharpe | Max DD | Trades | Win rate | In market |
|---|---:|---:|---:|---:|---:|---:|
| MeanReversion (lookback=20 entry_z=2 exit_z=0.5) | 7.5% | 0.75 | 15.4% | 26.00 | 80.6% | 23.3% |
| MACrossover (short_window=20 long_window=200) | 10.4% | 0.59 | 24.7% | 5.83 | 59.5% | 76.2% |
| BuyAndHold | 15.6% | 0.81 | 24.1% | 1 | - | 99.9% |

### Out-of-sample 2020-01-01 to 2024-12-31, average of 6 symbols

| Strategy | CAGR | Sharpe | Max DD | Trades | Win rate | In market |
|---|---:|---:|---:|---:|---:|---:|
| MeanReversion (lookback=20 entry_z=2 exit_z=0.5) | 4.5% | 0.30 | 33.7% | 19.83 | 64.8% | 24.4% |
| MACrossover (short_window=20 long_window=200) | 13.9% | 0.65 | 30.8% | 4.50 | 45.8% | 75.7% |
| BuyAndHold | 16.7% | 0.67 | 39.6% | 1 | - | 99.9% |

### Correlation of daily returns, MR vs MA, out-of-sample

| Symbol | Correlation |
|---|---:|
| AAPL | 0.58 |
| JPM | 0.40 |
| KO | 0.52 |
| MSFT | 0.54 |
| SPY | 0.53 |
| XOM | 0.26 |
| **Average** | 0.47 |

CSV results written to results/
