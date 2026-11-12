#!/usr/bin/env python3
"""Builds docs/index.html, the interactive results explorer, from the committed study outputs.

Usage: python3 -I scripts/build_explorer.py [--check]

Reads the summary and parameter-grid CSVs and the report tables under results/, and writes one
self-contained page from scripts/explorer_template.html. The page holds summary statistics only:
no prices, trades or daily equity, which Tiingo's terms don't allow redistributing.

With --check nothing is written, and the script fails if docs/index.html is out of date. Rerun
the script without --check after any change to results/ or to the template.
"""

import csv
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "scripts" / "explorer_template.html"
OUTPUT = ROOT / "docs" / "index.html"
PLACEHOLDER = "__DATA__"


def tables(path):
    """Returns the markdown tables in a report, keyed by their (## heading, ### heading)."""
    found = {}
    h2 = h3 = ""
    current = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("## "):
            h2, h3, current = line[3:].strip(), "", None
        elif line.startswith("### "):
            h3, current = line[4:].strip(), None
        elif line.startswith("|"):
            cells = [cell.strip() for cell in line.strip()[1:-1].split("|")]
            if set("".join(cells)) <= set("-: "):
                continue  # the |---|---| separator row
            if current is None:
                current = []
                found.setdefault((h2, h3), []).append(current)
            current.append(cells)
        else:
            current = None
    return found


def pct(text):
    return round(float(text.replace("%", "")) / 100, 6)


def bounds(text):
    match = re.fullmatch(r"(-?[\d.]+) to (-?[\d.]+)", text)
    return float(match.group(1)), float(match.group(2))


def summary_rows(path):
    with open(path, newline="", encoding="utf-8") as handle:
        return [
            {
                "window": row["window"],
                "st": row["strategy"],
                "sym": row["symbol"],
                "params": row.get("params", ""),
                "cagr": round(float(row["cagr"]), 5),
                "sharpe": round(float(row["sharpe"]), 4),
                "mdd": round(float(row["max_drawdown"]), 5),
                "trades": int(float(row["trades"])),
                "win": round(float(row["win_rate"]), 4),
                "exp": round(float(row["exposure"]), 4),
            }
            for row in csv.DictReader(handle)
        ]


def window(rows, name):
    return [{k: v for k, v in row.items() if k != "window"} for row in rows if row["window"] == name]


def grid(folder, stem, keys):
    """Average Sharpe across symbols for each parameter set, in and out of sample."""
    sharpes = {}
    for sample in ("in_sample", "out_of_sample"):
        with open(folder / f"{stem}_{sample}_grid.csv", newline="", encoding="utf-8") as handle:
            for row in csv.DictReader(handle):
                params = " ".join(f"{key}={row[key]}" for key in keys)
                sharpes.setdefault(params, {}).setdefault(sample, []).append(float(row["sharpe"]))
    return [
        {
            "p": params,
            "i": round(sum(by["in_sample"]) / len(by["in_sample"]), 4),
            "o": round(sum(by["out_of_sample"]) / len(by["out_of_sample"]), 4),
        }
        for params, by in sharpes.items()
    ]


def intervals(table):
    rows = []
    for cells in table[1:]:
        low, high = bounds(cells[2])
        rows.append({"cmp": cells[0], "gap": float(cells[1]), "lo": low, "hi": high,
                     "ahead": pct(cells[3])})
    return rows


def universe(label, short, study_dir, fresh_dir):
    study = summary_rows(study_dir / "summary.csv")
    fresh = summary_rows(fresh_dir / "summary.csv")
    report = tables(study_dir / "study_output.md")
    fresh_report = tables(fresh_dir / "test_output.md")

    by_year, compounded = report[("Robustness", "Walk-forward, 2016 to 2024")]
    walk = [
        {"year": int(c[0]), "mrP": c[1], "mr": float(c[2]), "maP": c[3], "ma": float(c[4]),
         "bh": float(c[5])}
        for c in by_year[1:]
    ]
    walk_summary = [
        {"name": c[0], "cagr": pct(c[1]), "sharpe": float(c[2]), "mdd": pct(c[3]), "years": c[4]}
        for c in compounded[1:]
    ]
    costs = [
        {"x": c[0], "mrC": pct(c[1]), "mrS": float(c[2]), "maC": pct(c[3]), "maS": float(c[4]),
         "bhC": pct(c[5]), "bhS": float(c[6])}
        for c in report[("Robustness", "Cost sensitivity, 2020 to 2024")][0][1:]
    ]
    hold = {
        strategy: {
            key: [
                {"held": c[0], "trades": int(c[1]), "win": pct(c[2]), "avg": pct(c[3])}
                for c in report[(strategy, f"Trades by holding period, {sample}")][0][1:]
            ]
            for key, sample in (("in", "in-sample"), ("out", "out-of-sample"))
        }
        for strategy in ("MeanReversion", "MACrossover")
    }
    hypotheses = []
    for c in fresh_report[("Pre-registered hypotheses", "")][0][1:]:
        low, high = bounds(c[4])
        hypotheses.append({"id": c[0], "text": c[1], "gap": float(c[3]), "lo": low, "hi": high,
                           "ahead": pct(c[5]), "verdict": c[6]})
    comparisons = fresh_report[
        ("Pre-registered hypotheses", "All comparisons (exploratory beyond H1-H4)")
    ][0]

    # Only the first two rows of each interval table compare a strategy with buy-and-hold
    return {
        "label": label,
        "short": short,
        "n": len({row["sym"] for row in study}),
        "periods": {
            "in": window(study, "in_sample"),
            "out": window(study, "out_of_sample"),
            "fresh": window(fresh, "test"),
        },
        "grid": {
            "MeanReversion": grid(study_dir, "mean_reversion", ["lookback", "entry_z", "exit_z"]),
            "MACrossover": grid(study_dir, "ma_crossover", ["short_window", "long_window"]),
        },
        "walk": walk,
        "walkSum": walk_summary,
        "costs": costs,
        "hold": hold,
        "hyp": hypotheses,
        "ci": {
            "out": intervals(report[("Robustness", "Confidence intervals, 2020 to 2024")][0])[:2],
            "wf": intervals(
                report[("Robustness", "Confidence intervals, walk-forward 2016 to 2024")][0]
            )[:2],
            "fresh": intervals(comparisons)[:2],
        },
    }


def build():
    results = ROOT / "results"
    data = {
        "universes": {
            "six": universe("Six hand-picked symbols", "Six symbols", results,
                            results / "phase7" / "six"),
            "dow": universe("29 Dow stocks as of January 2014", "29 Dow stocks",
                            results / "dow30", results / "phase7" / "dow29"),
        }
    }
    template = TEMPLATE.read_text(encoding="utf-8")
    if template.count(PLACEHOLDER) != 1:
        raise SystemExit(f"{TEMPLATE} must contain {PLACEHOLDER} exactly once")
    return template.replace(PLACEHOLDER, json.dumps(data, separators=(",", ":")))


def main(argv):
    if argv not in ([], ["--check"]):
        raise SystemExit(__doc__)
    page = build()
    if argv:
        if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != page:
            raise SystemExit(f"{OUTPUT.relative_to(ROOT)} is out of date: "
                             "run python3 -I scripts/build_explorer.py")
        print(f"{OUTPUT.relative_to(ROOT)} is up to date")
        return
    OUTPUT.write_text(page, encoding="utf-8")
    print(f"Wrote {OUTPUT.relative_to(ROOT)} ({len(page.encode('utf-8'))} bytes)")


if __name__ == "__main__":
    main(sys.argv[1:])
