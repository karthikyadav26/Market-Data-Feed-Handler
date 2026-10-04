#!/usr/bin/env bash
set -euo pipefail
BUILD_DIR="${1:-build}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="19100"
CAPTURE="$(mktemp --suffix=.bin)"
OUTPUT="$(mktemp --suffix=.csv)"
REPLAY="$(mktemp --suffix=.csv)"
LOG="$(mktemp)"
cleanup(){ rm -f "$CAPTURE" "$OUTPUT" "$REPLAY" "$LOG"; }
trap cleanup EXIT
"$BUILD_DIR/feed_handler" --port "$PORT" --mode pipelined --count 5 --record "$CAPTURE" --output "$OUTPUT" >"$LOG" 2>&1 &
PID=$!
sleep 0.2
"$BUILD_DIR/market_data_simulator" --input "$ROOT/data/sample_feed.csv" --count 5 --port "$PORT" --batch 1 >/dev/null
wait "$PID"
"$BUILD_DIR/feed_replay" --input "$CAPTURE" --mode max --output "$REPLAY" >/dev/null
cmp -s "$OUTPUT" "$REPLAY" || { echo "integration logical output mismatch" >&2; exit 1; }
if ! grep -q 'messages_decoded=5' "$LOG"; then cat "$LOG"; echo "expected five decoded messages" >&2; exit 1; fi
echo "Integration passed: simulator -> UDP/epoll -> decoder -> SPSC -> book -> recorder/replay."
