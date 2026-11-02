#!/usr/bin/env python3
"""Checks a decision-margin report from a golden run: both programs are in it, smallest first."""

import csv
import sys

PROGRAMS = {"strategy_study", "fresh_data_test"}


def main():
    with open(sys.argv[1], newline="") as f:
        rows = list(csv.DictReader(f))
    programs = {row["program"] for row in rows}
    if programs != PROGRAMS:
        sys.exit(f"Expected rows from {sorted(PROGRAMS)}, found {sorted(programs)}")
    margins = [float(row["smallest_margin"]) for row in rows]
    if margins != sorted(margins):
        sys.exit("Rows are not sorted by margin")
    if margins[0] < 0.0:
        sys.exit(f"Negative margin: {margins[0]}")
    print(f"{len(rows)} rows, smallest margin {margins[0]}")


if __name__ == "__main__":
    main()
