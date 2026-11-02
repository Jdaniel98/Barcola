#!/usr/bin/env python3
"""Tests the golden comparison script, which would otherwise pass silently if it were broken."""

import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "scripts"))

import compare_golden  # noqa: E402

SUMMARY = "window,symbol,cagr,sharpe,calmar\nw,AAA,0.1234567891,0.5,1.234567891\n"


class CompareGoldenTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.expected = pathlib.Path(temp.name) / "expected"
        self.actual = pathlib.Path(temp.name) / "actual"
        for folder in (self.expected, self.actual):
            (folder / "charts").mkdir(parents=True)
            (folder / "summary.csv").write_text(SUMMARY)
            (folder / "trades.csv").write_text("symbol,price\nAAA,1.5\n")
            (folder / "study_output.md").write_text("# Study\n")
            (folder / "charts" / "growth.svg").write_text("<svg/>\n")

    def compare(self):
        return compare_golden.compare(self.expected, self.actual)

    def write_summary(self, text):
        (self.actual / "summary.csv").write_text(text)

    def test_identical_passes(self):
        self.assertEqual(self.compare(), ([], []))

    def test_cagr_one_unit_up_warns(self):
        self.write_summary(SUMMARY.replace("0.1234567891", "0.1234567892"))
        failures, warnings = self.compare()
        self.assertEqual(failures, [])
        self.assertEqual(len(warnings), 1)
        self.assertIn("cagr", warnings[0])

    def test_calmar_one_unit_down_warns(self):
        self.write_summary(SUMMARY.replace("1.234567891", "1.234567890"))
        failures, warnings = self.compare()
        self.assertEqual(failures, [])
        self.assertEqual(len(warnings), 1)

    def test_cagr_two_units_fails(self):
        self.write_summary(SUMMARY.replace("0.1234567891", "0.1234567893"))
        failures, warnings = self.compare()
        self.assertEqual(len(failures), 1)
        self.assertEqual(warnings, [])

    def test_cagr_short_print_is_not_a_coarse_last_digit(self):
        # Trailing zeros are dropped, but 0.5 and 0.6 are still far apart
        self.write_summary(SUMMARY.replace("0.1234567891", "0.5"))
        (self.expected / "summary.csv").write_text(SUMMARY.replace("0.1234567891", "0.6"))
        self.assertEqual(len(self.compare()[0]), 1)

    def test_cagr_dropped_trailing_zero_within_one_unit_warns(self):
        self.write_summary(SUMMARY.replace("0.1234567891", "0.25"))
        (self.expected / "summary.csv").write_text(SUMMARY.replace("0.1234567891", "0.2500000001"))
        failures, warnings = self.compare()
        self.assertEqual(failures, [])
        self.assertEqual(len(warnings), 1)

    def test_cagr_across_a_power_of_ten_warns(self):
        self.write_summary(SUMMARY.replace("0.1234567891", "1"))
        (self.expected / "summary.csv").write_text(SUMMARY.replace("0.1234567891", "0.9999999999"))
        failures, warnings = self.compare()
        self.assertEqual(failures, [])
        self.assertEqual(len(warnings), 1)

    def test_cagr_nine_units_across_a_power_of_ten_fails(self):
        self.write_summary(SUMMARY.replace("0.1234567891", "1"))
        (self.expected / "summary.csv").write_text(SUMMARY.replace("0.1234567891", "0.9999999991"))
        self.assertEqual(len(self.compare()[0]), 1)

    def test_cagr_zero_against_nonzero_fails(self):
        self.write_summary(SUMMARY.replace("0.1234567891", "1e-10"))
        (self.expected / "summary.csv").write_text(SUMMARY.replace("0.1234567891", "0"))
        self.assertEqual(len(self.compare()[0]), 1)

    def test_cagr_not_a_number_fails(self):
        self.write_summary(SUMMARY.replace("0.1234567891", "nan"))
        self.assertEqual(len(self.compare()[0]), 1)

    def test_other_column_one_unit_fails(self):
        self.write_summary(SUMMARY.replace("0.5", "0.5000000001"))
        failures, warnings = self.compare()
        self.assertEqual(len(failures), 1)
        self.assertIn("sharpe", failures[0])
        self.assertEqual(warnings, [])

    def test_trades_any_difference_fails(self):
        (self.actual / "trades.csv").write_text("symbol,price\nAAA,1.500000001\n")
        self.assertEqual(len(self.compare()[0]), 1)

    def test_trades_with_a_cagr_column_still_fails(self):
        (self.expected / "trades.csv").write_text("cagr\n0.1234567891\n")
        (self.actual / "trades.csv").write_text("cagr\n0.1234567892\n")
        failures, warnings = self.compare()
        self.assertEqual(len(failures), 1)
        self.assertEqual(warnings, [])

    def test_markdown_difference_fails(self):
        (self.actual / "study_output.md").write_text("# Study!\n")
        self.assertEqual(len(self.compare()[0]), 1)

    def test_chart_difference_fails(self):
        (self.actual / "charts" / "growth.svg").write_text("<svg></svg>\n")
        self.assertEqual(len(self.compare()[0]), 1)

    def test_missing_file_fails(self):
        (self.actual / "trades.csv").unlink()
        self.assertEqual(len(self.compare()[0]), 1)

    def test_unexpected_file_fails(self):
        (self.actual / "extra.csv").write_text("a\n")
        self.assertEqual(len(self.compare()[0]), 1)

    def test_extra_row_fails(self):
        self.write_summary(SUMMARY + "w,BBB,0.1,0.2,0.3\n")
        self.assertEqual(len(self.compare()[0]), 1)

    def test_changed_header_fails(self):
        self.write_summary(SUMMARY.replace("sharpe", "sortino"))
        self.assertEqual(len(self.compare()[0]), 1)

    def test_line_endings_alone_fail(self):
        (self.actual / "summary.csv").write_bytes(SUMMARY.replace("\n", "\r\n").encode())
        self.assertEqual(len(self.compare()[0]), 1)


if __name__ == "__main__":
    unittest.main()
