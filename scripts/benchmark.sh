#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
for N in 100000 1000000 10000000; do
  "$BUILD/feed_benchmark" --messages "$N" --output "$ROOT/results/feed_benchmark.csv"
done
