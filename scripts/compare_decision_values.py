#!/usr/bin/env python3
"""Compares two outputs of apps/decision_values and states the threshold they imply.

Usage: python3 -I scripts/compare_decision_values.py <first.txt> <second.txt>

Reports the largest absolute difference in z-score and in the relative gaps. Phase 9c's
threshold is ten times the larger of the two, or 0 if the outputs are identical. Exits non-zero
if the files don't line up, since then the values can't be compared at all.
"""

import itertools
import sys

THRESHOLD_FACTOR = 10.0


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: compare_decision_values.py <first.txt> <second.txt>")

    largest = {"z": 0.0, "gap": 0.0}
    where = {"z": "", "gap": ""}
    compared = 0
    differing = 0
    kind = None
    header = ""
    with open(sys.argv[1]) as first, open(sys.argv[2]) as second:
        for line_a, line_b in itertools.zip_longest(first, second):
            if line_a is None or line_b is None:
                sys.exit("One file is longer than the other")
            if line_a.startswith("#"):
                if line_a != line_b:
                    sys.exit(f"Series don't match: {line_a.strip()!r} and {line_b.strip()!r}")
                header = line_a[2:].strip()
                kind = "z" if header.split()[1] == "z" else "gap"
                continue
            compared += 1
            if line_a == line_b:
                continue
            differing += 1
            difference = abs(float.fromhex(line_a) - float.fromhex(line_b))
            if difference > largest[kind]:
                largest[kind] = difference
                where[kind] = header

    print(f"Values compared: {compared}, differing: {differing}")
    print(f"Largest z-score difference: {largest['z']:.6e} {where['z']}")
    print(f"Largest relative gap difference: {largest['gap']:.6e} {where['gap']}")
    print(f"Threshold (10 x the larger): {THRESHOLD_FACTOR * max(largest.values()):.6e}")


if __name__ == "__main__":
    main()
