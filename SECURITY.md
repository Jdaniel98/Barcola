# Security Policy

## Supported versions

Only the latest 2.x release gets security fixes.

| Version | Supported |
|---|---|
| Latest 2.x | Yes |
| Anything older | No |

## Reporting a vulnerability

Please report it privately through GitHub's private vulnerability reporting: open this repository's **Security** tab and choose **Report a vulnerability**. Please don't open a public issue.

Include what is affected, how to reproduce it, and what an attacker could do with it.

## Scope

- **Library:** `src/` and `strategies/`, especially the CSV loader, which is the only input from outside the program.
- **Scripts:** `scripts/`, including how `fetch_market_data.sh` handles the Tiingo token.
- **CI:** `.github/workflows/` and the pinned build dependencies.

Tiingo's own service and the accuracy of research conclusions are out of scope.

## What to expect

Barcola has one maintainer, so responses are best-effort, with no guaranteed response time and no bug bounty.

## Not investment advice

Barcola is a research library. It places no orders, holds no money and gives no investment advice.
