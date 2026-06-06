#!/usr/bin/env bash
# Build + run the host unit test for the slim launcher's version matching.
# Uses the host C compiler (no devkitPro / libnx needed).
set -euo pipefail
cd "$(dirname "$0")"

CC="${CC:-cc}"
OUT="$(mktemp -d)/match-test"

"$CC" -std=gnu11 -Wall -Wextra -I../source \
	-o "$OUT" \
	match-test.c ../source/match.c ../source/vendor/semver.c

"$OUT"
