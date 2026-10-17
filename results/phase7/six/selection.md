# Phase 7 parameter selection, six (6 symbols)

Rule: highest average Sharpe ratio across symbols on 2022-01-01 to 2024-12-31.

## Mean reversion

| Rank | Parameters | Mean Sharpe |
|---:|---|---:|
| 1 | lookback=30 entry_z=2.5 exit_z=0 | 0.84 |
| 2 | lookback=30 entry_z=2.5 exit_z=0.5 | 0.84 |
| 3 | lookback=30 entry_z=2 exit_z=0 | 0.80 |

Recorded in preregistration.h: lookback=30 entry_z=2.5 exit_z=0 (matches)

## MA crossover

| Rank | Parameters | Mean Sharpe |
|---:|---|---:|
| 1 | short_window=20 long_window=200 | 0.55 |
| 2 | short_window=10 long_window=200 | 0.49 |
| 3 | short_window=50 long_window=200 | 0.46 |

Recorded in preregistration.h: short_window=20 long_window=200 (matches)

## Mean reversion + trend filter

| Rank | Parameters | Mean Sharpe |
|---:|---|---:|
| 1 | lookback=30 entry_z=2 exit_z=0 | 0.71 |
| 2 | lookback=30 entry_z=2.5 exit_z=0 | 0.69 |
| 3 | lookback=30 entry_z=2 exit_z=0.5 | 0.68 |

Recorded in preregistration.h: lookback=30 entry_z=2 exit_z=0 (matches)

## Mean reversion + time stop

| Rank | Parameters | Mean Sharpe |
|---:|---|---:|
| 1 | lookback=30 entry_z=2 exit_z=0 | 0.84 |
| 2 | lookback=30 entry_z=2.5 exit_z=0.5 | 0.80 |
| 3 | lookback=30 entry_z=2.5 exit_z=0 | 0.78 |

Recorded in preregistration.h: lookback=30 entry_z=2 exit_z=0 (matches)

For preregistration.h:

```cpp
    {"six", "MeanReversion", {30, 2.5, 0}},
    {"six", "MACrossover", {20, 200}},
    {"six", "MeanReversionTrend", {30, 2, 0}},
    {"six", "MeanReversionTimeStop", {30, 2, 0}},
```
