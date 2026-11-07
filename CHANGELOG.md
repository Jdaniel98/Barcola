# Changelog

All notable changes to Barcola are recorded here. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.1] - 2026-11-07

### Changed

- Builds now use `-ffp-contract=off`, so a compiler can't fuse `a*b + c` into a single multiply-add that rounds once instead of twice. Fused instructions were in use on both CI platforms, in the z-score, the Sharpe and Sortino ratios, percentiles and the backtest engine.
- CMake now refuses `-ffast-math`, `-Ofast`, `-fassociative-math` and `-funsafe-math-optimizations`.
- The README's reproducibility claim now covers other platforms and states the maths-library exception: CAGR and Calmar use the platform's `pow()`, which can differ in the last digit.

Published results are unchanged: rerunning both study programs on the real data reproduced all 32 output files byte for byte. That rerun was on an Intel Mac, whose builds never used fused instructions, so the flag changes nothing there; a separate build with them forced on reproduced the same 32 files. The real data has not been run on arm64.

### Added

- Golden outputs: both study programs run on committed synthetic prices in `data/golden/`, and CI checks the results against `tests/golden/` on Linux and macOS.
- A decision-margin report, written when `BARCOLA_MARGIN_REPORT=<path>` is set. It shows how close each run came to deciding the other way on some bar, without changing any result.
- `apps/decision_values`, which prints the values behind every decision so two builds can be compared.

### Security

- Pinned every GitHub Action and GoogleTest to a full commit SHA, and added Dependabot updates for the action pins.
- Limited the CI workflow's token to read-only.
- Added `SECURITY.md` and an OpenSSF Scorecard workflow, which runs once the repository is public.
- Added a libFuzzer target for `PriceHistory::loadFromCsv`, run for 60 seconds in CI under ASan and UBSan.
- `scripts/fetch_market_data.sh` now gives curl the Tiingo token on stdin, keeping it off the command line, where other users could see it in a process listing.

## [2.0.0] - 2026-10-21

### Removed

- `DynamicHedging`. It only computed and printed two moving averages, which `indicators/moving_average` already provides.
- `PriceHistory::fetchHistoricalData`. Yahoo's endpoint now refuses unauthenticated downloads; use `loadFromCsv` with `scripts/fetch_market_data.sh` instead.

There was no deprecation release first: Barcola has no known downstream users, and one of the two APIs no longer worked.

### Changed

- libcurl is no longer required to build.

## [1.0.0] - 2026-10-17

### Added

- Phase 1: Foundation hardening (CMake, tests, CI, bug fixes, CSV support)
- Phase 2: Backtesting engine (strategy interface, engine, performance metrics)
- Phase 3: Mean reversion backtest with documented results
- Phase 4: Second strategy (dual MA crossover) + comparison
- Phase 5: Charts and a results-first README
- Phase 6: Robustness: bootstrap intervals, walk-forward, cost sensitivity, 29 Dow stocks
- Phase 7: Pre-registered test on fresh 2025–2026 data
- Phase 8: Release polish: MIT license, stricter CI (warnings as errors, sanitizers, formatting, lint, coverage), v1.0

[Unreleased]: https://github.com/Jdaniel98/Barcola/compare/v2.0.1...HEAD
[2.0.1]: https://github.com/Jdaniel98/Barcola/compare/v2.0.0...v2.0.1
[2.0.0]: https://github.com/Jdaniel98/Barcola/compare/v1.0.0...v2.0.0
[1.0.0]: https://github.com/Jdaniel98/Barcola/releases/tag/v1.0.0
