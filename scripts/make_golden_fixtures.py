#!/usr/bin/env python3
"""Generates the synthetic prices in data/golden/, one <TICKER>.csv per pre-registered symbol.

Run once, as python3 -I scripts/make_golden_fixtures.py, and commit the output. random.gauss
and math.exp go through the platform's maths library, so another machine may not produce the
same bytes. CI never runs this. Regenerate only deliberately; the golden outputs in
tests/golden/ depend on these exact files.

The prices are a random walk with occasional jumps. They are not market data; the tickers are
the ones fresh_data_test accepts.
"""

import datetime
import math
import pathlib
import random

SEED = 20261102
TICKERS = ["AAPL", "JPM", "KO", "MSFT", "SPY", "XOM"]
FIRST_DATE = datetime.date(2012, 1, 3)
LAST_DATE = datetime.date(2026, 9, 30)
WEEKDAYS = 5  # date.weekday() below this is Monday to Friday

START_PRICE = 100.0
DAILY_DRIFT = 0.0003
DAILY_VOLATILITY = 0.014
OVERNIGHT_VOLATILITY = 0.003
INTRADAY_RANGE = 0.006  # scale of how far the high and low reach past the open and close
JUMP_CHANCE = 0.005
JUMP_VOLATILITY = 0.06
TICK = 0.0001  # prices are written to 4 decimal places
MIN_PRICE = 1.0


def weekdays():
    date = FIRST_DATE
    while date <= LAST_DATE:
        if date.weekday() < WEEKDAYS:
            yield date
        date += datetime.timedelta(days=1)


def rows_for_ticker(rng):
    close = START_PRICE
    for date in weekdays():
        move = rng.gauss(DAILY_DRIFT, DAILY_VOLATILITY)
        if rng.random() < JUMP_CHANCE:
            move += rng.gauss(0.0, JUMP_VOLATILITY)
        opening = max(close * math.exp(rng.gauss(0.0, OVERNIGHT_VOLATILITY)), MIN_PRICE)
        close = max(close * math.exp(move), MIN_PRICE)
        high = max(opening, close) * (1.0 + abs(rng.gauss(0.0, INTRADAY_RANGE)))
        low = min(opening, close) * (1.0 - abs(rng.gauss(0.0, INTRADAY_RANGE)))

        opening, close = round(opening, 4), round(close, 4)
        # Rounding must not break high >= max(open, close) or low <= min(open, close)
        high = max(round(high, 4), opening, close)
        low = max(min(round(low, 4), opening, close), TICK)
        yield date, opening, high, low, close


def main():
    out_dir = pathlib.Path(__file__).resolve().parent.parent / "data" / "golden"
    out_dir.mkdir(parents=True, exist_ok=True)
    rng = random.Random(SEED)
    for ticker in TICKERS:
        with open(out_dir / f"{ticker}.csv", "w", newline="\n") as out:
            out.write("Date,Open,High,Low,Close\n")
            for date, opening, high, low, close in rows_for_ticker(rng):
                out.write(f"{date},{opening:.4f},{high:.4f},{low:.4f},{close:.4f}\n")


if __name__ == "__main__":
    main()
