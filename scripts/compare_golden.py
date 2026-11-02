#!/usr/bin/env python3
"""Compares a golden study run with the committed golden outputs.

Usage: python3 -I scripts/compare_golden.py <expected_dir> <actual_dir>

Every file must be byte-identical, with one exception: CAGR and Calmar go through the platform's
pow(), which isn't correctly rounded. A CSV may differ in a cagr or calmar column by at most one
unit in the last printed significant digit, and each such difference is printed as a warning.
Anything else fails: trades.csv, any other column, any .md file and any .svg file. Don't loosen
the rule to make a failure pass; investigate it.
"""

import csv
import decimal
import pathlib
import sys

POW_COLUMNS = {"cagr", "calmar"}
PRINTED_DIGITS = 10  # study_common's openOutput writes CSVs with setprecision(10)
TRADES_FILE = "trades.csv"


def files_under(root):
    return {path.relative_to(root) for path in root.rglob("*") if path.is_file()}


def within_one_last_digit(expected, actual):
    """True if two printed numbers differ by at most 1 in their last significant digit."""
    try:
        a = decimal.Decimal(expected)
        b = decimal.Decimal(actual)
    except decimal.InvalidOperation:
        return False
    if not a.is_finite() or not b.is_finite() or a.is_zero() or b.is_zero():
        return False
    # Printing drops trailing zeros, so 0.5 can stand for 0.5000000000: the last digit is always
    # the PRINTED_DIGITS-th, whatever the text shows. Across a power of ten, as with 0.9999999999
    # and 1, the smaller number's digit is the finer one, and the two are one unit of it apart.
    unit = decimal.Decimal(1).scaleb(min(a.adjusted(), b.adjusted()) - (PRINTED_DIGITS - 1))
    with decimal.localcontext() as context:
        context.prec = 2 * PRINTED_DIGITS
        return abs(a - b) <= unit


def csv_differences(relative, expected_path, actual_path):
    """Returns (failures, warnings) for a CSV that isn't byte-identical."""
    if relative.name == TRADES_FILE:
        return [f"{relative}: trades differ"], []

    with open(expected_path, newline="") as f:
        expected = list(csv.reader(f))
    with open(actual_path, newline="") as f:
        actual = list(csv.reader(f))

    if len(expected) != len(actual):
        return [f"{relative}: {len(expected)} rows expected, {len(actual)} found"], []
    if not expected or expected[0] != actual[0]:
        return [f"{relative}: header differs"], []

    failures = []
    warnings = []
    header = expected[0]
    for number, (row_e, row_a) in enumerate(zip(expected[1:], actual[1:]), start=2):
        if len(row_e) != len(row_a) or len(row_e) != len(header):
            failures.append(f"{relative} line {number}: wrong number of columns")
            continue
        for column, value_e, value_a in zip(header, row_e, row_a):
            if value_e == value_a:
                continue
            message = f"{relative} line {number}, {column}: expected {value_e}, found {value_a}"
            if column in POW_COLUMNS and within_one_last_digit(value_e, value_a):
                warnings.append(message)
            else:
                failures.append(message)
    if not failures and not warnings:
        # The parsed fields match, so the bytes differ in something else, such as line endings
        failures.append(f"{relative}: differs outside its fields")
    return failures, warnings


def compare(expected_dir, actual_dir):
    """Returns (failures, warnings) for the two folders."""
    expected_files = files_under(expected_dir)
    actual_files = files_under(actual_dir)
    failures = [f"{path}: missing" for path in sorted(expected_files - actual_files)]
    failures += [f"{path}: unexpected" for path in sorted(actual_files - expected_files)]
    warnings = []
    for relative in sorted(expected_files & actual_files):
        expected_path = expected_dir / relative
        actual_path = actual_dir / relative
        if expected_path.read_bytes() == actual_path.read_bytes():
            continue
        if relative.suffix == ".csv":
            new_failures, new_warnings = csv_differences(relative, expected_path, actual_path)
            failures += new_failures
            warnings += new_warnings
        else:
            failures.append(f"{relative}: differs")
    return failures, warnings


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: compare_golden.py <expected_dir> <actual_dir>")
    expected_dir = pathlib.Path(sys.argv[1])
    actual_dir = pathlib.Path(sys.argv[2])
    for folder in (expected_dir, actual_dir):
        if not folder.is_dir():
            sys.exit(f"{folder} is not a folder")

    failures, warnings = compare(expected_dir, actual_dir)
    for warning in warnings:
        print(f"warning: {warning}")
    for failure in failures:
        print(f"FAIL: {failure}")
    if failures:
        sys.exit(f"Golden outputs differ: {len(failures)} failures, {len(warnings)} warnings")
    print(f"Golden outputs match ({len(warnings)} last-digit warnings in cagr or calmar)")


if __name__ == "__main__":
    main()
