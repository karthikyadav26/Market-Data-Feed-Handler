#!/usr/bin/env bash
set -euo pipefail
BUILD_DIR="${1:-build}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT
"$BUILD_DIR/feed_replay" --input "$ROOT/tests/golden/session.bin" --mode max --output "$TMP" >/dev/null
cmp -s "$TMP" "$ROOT/tests/golden/expected_output.csv" || {
  echo "REGRESSION FAILED: logical output changed" >&2
  diff -u "$ROOT/tests/golden/expected_output.csv" "$TMP" || true
  exit 1
}
echo "Regression passed: golden output is identical."
