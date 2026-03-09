# Barcola — Project Conventions

## What This Is
A C++17 quantitative trading toolkit being transformed from a learning project into a
quant-grade portfolio piece. The end goal: a fully backtested mean reversion strategy
on real market data, with verified math, professional engineering, and documented results.

## Architecture
- `src/core/` — Foundational types: PricePoint, PriceHistory, time utilities
- `src/indicators/` — Technical indicators: SMA, EMA, RSI, Bollinger Bands
- `src/analysis/` — Market analysis: mean reversion, correlation, market data analysis
- `src/risk/` — Risk management: position sizing, slippage modeling, dynamic hedging
- `src/simulation/` — Monte Carlo simulation
- `src/backtest/` — (Phase 2) Backtesting engine, strategy interface, performance metrics
- `tests/` — Google Test suite, one test file per module
- `data/` — Static CSV files for reproducible testing (never depend on network for tests)

## Build
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## C++ Standards & Idioms
- **C++17 required.** Use `std::optional`, `std::string_view`, structured bindings,
  `[[nodiscard]]`, `if constexpr` where appropriate.
- **No raw `new`/`delete`.** Stack-allocate by default. Use `std::unique_ptr` only when
  polymorphism or heap allocation is genuinely needed.
- **Const correctness.** All getters are `const`. Pass by `const&` unless mutation is needed.
  Prefer `const` local variables.
- **RAII everywhere.** Resources (CURL handles, file handles) are managed by constructors/
  destructors or scope guards, never manual cleanup.
- **No `using namespace std;` in headers.** Allowed in .cpp files at function scope only.
- **`#pragma once`** for all headers.
- **Error handling:** Throw `std::runtime_error` or `std::invalid_argument` for unrecoverable
  errors. Return `std::optional` for expected-empty cases. Never silently swallow failures.
- **No magic numbers.** Use named constants (`constexpr double RISK_FREE_RATE = 0.05;`).

## Math Correctness (Critical)
- Every numerical function must handle edge cases: division by zero, empty inputs,
  insufficient data points, NaN/infinity propagation.
- All formulas must be verifiable against hand-calculated known values in tests.
- When in doubt, return a safe sentinel (0.0 for undefined correlation, 100.0 for RSI
  with no losses) and document the choice.

## Testing Conventions
- One test file per module: `tests/test_<module>.cpp`
- Use `EXPECT_DOUBLE_EQ` for exact floating-point matches, `EXPECT_NEAR(a, b, 1e-10)`
  for computed values with rounding.
- Every bug fix gets a regression test that would have failed before the fix.
- Test data comes from CSV files in `data/`, path injected via `BARCOLA_TEST_DATA_DIR`
  compile definition. Tests must NEVER hit the network.
- Name tests descriptively: `TEST(RSITest, AllGains_Returns100)`, not `TEST(RSI, Test1)`.

## Git Conventions
- Feature branches: `phase-N/description` (e.g., `phase-1/foundation-hardening`)
- Commit messages: imperative mood, concise.
- Each commit should compile and pass tests.

## What NOT to Do
- Do not add external trading libraries. Everything is built from scratch.
- Do not depend on Yahoo Finance API for tests or CI. Use CSV files.
- Do not optimize prematurely. Correctness first, then clarity, then performance.
- Do not add features from later phases during Phase 1.

## Roadmap Reference
Phase 1: Foundation hardening (CMake, tests, CI, bug fixes, CSV support)
Phase 2: Backtesting engine (strategy interface, backtest engine, performance metrics)
Phase 3: Run mean reversion backtest + document results
Phase 4: Second strategy (dual MA crossover) + comparison
Phase 5: Polish, visualizations, README rewrite
