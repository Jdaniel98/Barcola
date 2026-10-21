# Changelog

All notable changes to Barcola are recorded here. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/Jdaniel98/Barcola/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/Jdaniel98/Barcola/compare/v1.0.0...v2.0.0
[1.0.0]: https://github.com/Jdaniel98/Barcola/releases/tag/v1.0.0
