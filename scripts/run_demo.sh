#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
CAPTURE="$ROOT/captures/demo_session.bin"
OUTPUT="$ROOT/captures/demo_output.csv"
rm -f "$CAPTURE" "$OUTPUT"
"$BUILD/feed_handler" --port 9000 --mode pipelined --count 5 --record "$CAPTURE" --output "$OUTPUT" &
HANDLER_PID=$!
sleep 0.2
"$BUILD/market_data_simulator" --input "$ROOT/data/sample_feed.csv" --count 5 --port 9000 --batch 1
wait "$HANDLER_PID"
"$BUILD/feed_replay" --input "$CAPTURE" --mode max --output "$ROOT/captures/replay_output.csv"
echo "Demo complete. Outputs are under captures/."
