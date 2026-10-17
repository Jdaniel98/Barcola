#!/usr/bin/env bash
# Runs clang-tidy (.clang-tidy) on every C++ source file tracked by git, using the compile
# commands of an already configured build directory. Any finding fails the run.
#
# Usage: scripts/run_clang_tidy.sh [BUILD_DIR]     (default: build)
# CI uses clang-tidy 18.1.8 (pip install clang-tidy==18.1.8); set CLANG_TIDY to pick a binary.
set -euo pipefail

cd "$(dirname "$0")/.."
build_dir=${1:-build}
clang_tidy=${CLANG_TIDY:-clang-tidy}
jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN)}

if [[ ! -f "$build_dir/compile_commands.json" ]]; then
    echo "error: no $build_dir/compile_commands.json; configure first: cmake -B $build_dir" >&2
    exit 1
fi

"$clang_tidy" --version | grep -i version
# clang-tidy counts the warnings it suppressed in system headers on stderr; drop that noise
git ls-files -z '*.cpp' |
    xargs -0 -n 1 -P "$jobs" "$clang_tidy" -p "$build_dir" --quiet \
        2> >(grep -v -E '^[0-9]+ warnings? (and [0-9]+ errors? )?generated\.$' >&2)
echo "clang-tidy: no findings"
