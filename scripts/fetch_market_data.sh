#!/usr/bin/env bash
# Downloads daily prices from Tiingo into data/market/<SYMBOL>.csv.
#
# Usage: scripts/fetch_market_data.sh [SYMBOL ...]
#   Defaults to the Phase 3 study universe: SPY AAPL MSFT JPM XOM KO.
#
# Needs a free Tiingo API token (https://www.tiingo.com/account/api/token). The script
# reads it from TIINGO_API_TOKEN or asks for it without echoing; it is only sent in the
# Authorization header and never written to disk. Needs curl and python3.
#
# Output columns are Date,Open,High,Low,Close,Adj Close. PriceHistory::loadFromCsv scales
# the raw prices by Adj Close / Close, so splits and dividends are accounted for.
set -euo pipefail

START_DATE="2012-01-01"  # two years of history before 2014, enough to form 200-day averages
END_DATE="2024-12-31"
API="https://api.tiingo.com"

if [ $# -eq 0 ]; then
    set -- SPY AAPL MSFT JPM XOM KO
fi

OUT_DIR="$(cd "$(dirname "$0")/.." && pwd)/data/market"
mkdir -p "$OUT_DIR"

if [ -z "${TIINGO_API_TOKEN:-}" ]; then
    read -r -s -p "Tiingo API token: " TIINGO_API_TOKEN
    echo
fi
# Drop whitespace a paste may have picked up
TIINGO_API_TOKEN="$(printf '%s' "$TIINGO_API_TOKEN" | tr -d '[:space:]')"
if [ -z "$TIINGO_API_TOKEN" ]; then
    echo "No token entered." >&2
    exit 1
fi

response="$(mktemp)"
trap 'rm -f "$response"' EXIT

# GET $1 into $response and print the HTTP status.
fetch() {
    curl -sS -o "$response" -w '%{http_code}' \
        -H "Content-Type: application/json" \
        -H "Authorization: Token ${TIINGO_API_TOKEN}" \
        "$1"
}

# 1. Check the token before downloading anything. This endpoint answers HTTP 200 either
#    way; only the message says whether the token was accepted.
status="$(fetch "${API}/api/test")"
if [ "$status" != "200" ] || ! grep -q "successfully" "$response"; then
    echo "Tiingo rejected the token (HTTP ${status}): $(head -c 200 "$response")" >&2
    echo "Check that you copied the whole token from https://www.tiingo.com/account/api/token" >&2
    echo "and that your Tiingo account's email address is confirmed." >&2
    exit 1
fi
echo "Token accepted."

# 2. Download each symbol as JSON and convert it to CSV
for symbol in "$@"; do
    status="$(fetch "${API}/tiingo/daily/${symbol}/prices?startDate=${START_DATE}&endDate=${END_DATE}")"
    if [ "$status" != "200" ]; then
        echo "${symbol}: download failed (HTTP ${status}): $(head -c 300 "$response")" >&2
        exit 1
    fi

    python3 - "$response" "${OUT_DIR}/${symbol}.csv" <<'PY'
import json
import sys

source, target = sys.argv[1], sys.argv[2]
with open(source) as f:
    rows = json.load(f)
if not isinstance(rows, list) or not rows:
    sys.exit(f"unexpected response: {str(rows)[:200]}")

with open(target, "w") as out:
    out.write("Date,Open,High,Low,Close,Adj Close\n")
    for row in rows:
        out.write(f"{row['date'][:10]},{row['open']},{row['high']},{row['low']},"
                  f"{row['close']},{row['adjClose']}\n")
PY

    echo "${symbol}: $(($(wc -l < "${OUT_DIR}/${symbol}.csv") - 1)) rows"
done
