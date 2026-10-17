#!/usr/bin/env bash
# Checks that every C++ file tracked by git matches .clang-format, skipping the files listed
# in .clang-format-ignore. With --fix, reformats them in place instead.
#
# Usage: scripts/check_format.sh [--fix]
# Formatting differs between clang-format versions, so this requires the one CI uses:
#   pip install clang-format==18.1.8      (or set CLANG_FORMAT to that binary)
set -euo pipefail

cd "$(dirname "$0")/.."
REQUIRED_VERSION=18.1.8
clang_format=${CLANG_FORMAT:-clang-format}

version=$("$clang_format" --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1 ||
          true)
if [[ "$version" != "$REQUIRED_VERSION" ]]; then
    echo "error: need clang-format $REQUIRED_VERSION, found ${version:-none}" >&2
    echo "       pip install clang-format==$REQUIRED_VERSION" >&2
    exit 1
fi

if [[ "${1:-}" == "--fix" ]]; then
    git ls-files -z '*.cpp' '*.h' | xargs -0 "$clang_format" -i
    echo "Formatted."
else
    git ls-files -z '*.cpp' '*.h' | xargs -0 "$clang_format" --dry-run --Werror
    echo "Formatting OK."
fi
