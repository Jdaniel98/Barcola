# Phase 7 parameter selection, dow29 (29 symbols)

Rule: highest average Sharpe ratio across symbols on 2022-01-01 to 2024-12-31.

## Mean reversion

| Rank | Parameters | Mean Sharpe |
|---:|---|---:|
| 1 | lookback=50 entry_z=1 exit_z=0.5 | 0.41 |
| 2 | lookback=50 entry_z=2.5 exit_z=0 | 0.41 |
| 3 | lookback=50 entry_z=2.5 exit_z=0.5 | 0.40 |

Recorded in preregistration.h: lookback=50 entry_z=1 exit_z=0.5 (matches)

## MA crossover

| Rank | Parameters | Mean Sharpe |
|---:|---|---:|
| 1 | short_window=20 long_window=200 | 0.22 |
| 2 | short_window=50 long_window=200 | 0.20 |
| 3 | short_window=10 long_window=200 | 0.17 |

Recorded in preregistration.h: short_window=20 long_window=200 (matches)

## Mean reversion + trend filter

| Rank | Parameters | Mean Sharpe |
|---:|---|---:|
| 1 | lookback=30 entry_z=2.5 exit_z=0.5 | 0.42 |
| 2 | lookback=50 entry_z=1 exit_z=0.5 | 0.41 |
| 3 | lookback=30 entry_z=2.5 exit_z=0 | 0.37 |

Recorded in preregistration.h: lookback=30 entry_z=2.5 exit_z=0.5 (matches)

## Mean reversion + time stop

| Rank | Parameters | Mean Sharpe |
|---:|---|---:|
| 1 | lookback=50 entry_z=1 exit_z=0.5 | 0.40 |
| 2 | lookback=20 entry_z=2.5 exit_z=0.5 | 0.33 |
| 3 | lookback=50 entry_z=1 exit_z=0 | 0.31 |

Recorded in preregistration.h: lookback=50 entry_z=1 exit_z=0.5 (matches)

For preregistration.h:

```cpp
    {"dow29", "MeanReversion", {50, 1, 0.5}},
    {"dow29", "MACrossover", {20, 200}},
    {"dow29", "MeanReversionTrend", {30, 2.5, 0.5}},
    {"dow29", "MeanReversionTimeStop", {50, 1, 0.5}},
```
