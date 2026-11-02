#!/usr/bin/env bash
# Runs both study programs on the synthetic prices in data/golden.
#
# Usage: scripts/run_golden.sh <build_dir> <out_dir>
#
# Each program runs from inside <out_dir> with a relative results folder, so the paths it
# prints are the same wherever the output goes. Both results folders are emptied first, so a file
# the programs stop writing can't survive and pass. equity_out_of_sample.csv is deleted: it is
# large, and the trades and metrics already cover it.
set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: scripts/run_golden.sh <build_dir> <out_dir>" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$(cd "$1" && pwd)"
rm -rf "${2:?}/strategy_study" "${2:?}/fresh_data_test"
mkdir -p "$2/strategy_study" "$2/fresh_data_test"
cd "$2"

"$build_dir/apps/strategy_study" "$repo_root/data/golden" strategy_study "golden" \
    > strategy_study/study_output.md
"$build_dir/apps/fresh_data_test" run "$repo_root/data/golden" fresh_data_test six \
    > fresh_data_test/test_output.md
rm strategy_study/equity_out_of_sample.csv
