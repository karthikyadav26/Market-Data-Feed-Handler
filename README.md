# Market Data Feed Handler & Replay Engine

A complete educational Linux/C++20 stack for binary market-data ingestion, validation, local order-book maintenance, recording, deterministic replay, regression testing, and benchmarking.

> **Scope:** This is a custom ITCH-style binary protocol inspired by exchange-feed engineering patterns. It is not NSE/BSE/NYSE/Nasdaq ITCH and does not connect to a real exchange. The transport is ordinary Linux UDP. DPDK is not used in the default build.

## What a market-data feed handler does

A feed handler receives a high-rate stream of exchange-like events, converts the compact wire representation into application objects, validates ordering and message integrity, and hands events to the downstream trading or market-data stack. Binary protocols are useful because they make field widths explicit and avoid the parsing overhead and ambiguity of text/JSON formats on the wire.

## Architecture

```text
Simulator
   |
   | UDP localhost
   v
UdpTransport -> epoll/non-blocking receive -> Decoder -> Sequence Validator
                                                   |
                                                   v
                                           SPSC Queue (Mode B)
                                                   |
                                                   v
                                           Local Order Book
                                              /         \\
                                             v           v
                                         Recorder     CSV output

Capture file -> Replay -> same decoder -> same sequence/book rules -> same logical CSV
```

## Build

Requirements: Ubuntu/Linux or WSL2 with GCC/Clang, CMake 3.16+, pthreads, and standard Linux headers/tools.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Or:

```bash
./scripts/build.sh
```

Default build does **not** require DPDK.

## Binaries and CLIs

Every binary supports `--help`.

```text
market_data_simulator   deterministic UDP source
feed_handler            live UDP receiver + decoder + sequence validation + book
feed_recorder           offline CSV-to-capture recorder for deterministic fixtures
feed_replay             capture reader + replay through decoder/book
feed_benchmark          reproducible in-memory throughput/latency benchmark
```

## Sample input

`data/sample_feed.csv` uses:

```text
sequence,type,instrument_id,order_id,side,price,quantity,timestamp_ns
1,ADD_ORDER,1,1001,BUY,10100,100,1000000000
2,ADD_ORDER,1,1002,SELL,10120,50,1000001000
3,EXECUTE_ORDER,1,1001,BUY,10100,30,1000002000
4,CANCEL_ORDER,1,1002,SELL,10120,50,1000003000
5,REPLACE_ORDER,1,1001,BUY,10105,80,1000004000
```

## Launch the simulator

Generated deterministic feed:

```bash
./build/market_data_simulator --count 1000000 --rate max --port 9000
```

CSV input:

```bash
./build/market_data_simulator --input data/feed.csv --port 9000 --batch 1
```

Intentional gap for testing:

```bash
./build/market_data_simulator --count 10000 --gap-at 5000 --port 9000
```

Malformed-packet testing:

```bash
./build/market_data_simulator --count 10000 --malformed-every 1000 --port 9000
```

Multiple messages per UDP packet:

```bash
./build/market_data_simulator --count 100000 --batch 8 --port 9000
```

## Launch the feed handler

Single-threaded:

```bash
./build/feed_handler --port 9000 --mode single --output captures/live_output.csv
```

Pipelined:

```bash
./build/feed_handler --port 9000 --mode pipelined --record captures/session.bin --output captures/live_output.csv
```

Top-5 depth:

```bash
./build/feed_handler --port 9000 --mode pipelined --depth 5 --output captures/depth5.csv
```

Latency CSV:

```bash
./build/feed_handler --port 9000 --mode pipelined \
  --output captures/live_output.csv \
  --latency-output captures/latency.csv
```

The handler prints metrics such as received/decoded/dropped messages, gaps, duplicates, out-of-order messages, decode errors, queue drops, and failed book updates. The default SPSC policy uses backpressure rather than silently dropping queue entries.

## Record a feed

```bash
./build/feed_handler --port 9000 --mode pipelined \
  --count 100000 \
  --record captures/session.bin \
  --output captures/session_output.csv
```

The capture stores the original binary message bytes and receive timestamps. It does not store only the final book state.

## Record a deterministic offline fixture

For a capture that can be replayed without UDP, convert the human-readable sample feed directly into the framed capture format:

```bash
./build/feed_recorder --input data/feed.csv --output captures/offline_session.bin
```

This utility uses the source timestamp plus the optional `--receive-offset-ns` as a deterministic stand-in for receive time. Live captures should use `feed_handler --record` so the receive timestamp is taken from the Linux monotonic clock.

## Replay a feed

Max speed:

```bash
./build/feed_replay --input captures/session.bin --mode max --output captures/replay_output.csv
```

Realtime:

```bash
./build/feed_replay --input captures/session.bin --mode realtime --speed 1 --output captures/replay_output.csv
```

10x:

```bash
./build/feed_replay --input captures/session.bin --mode realtime --speed 10 --output captures/replay_10x.csv
```

Replay is deterministic at the logical event/book-output level. Realtime pacing intentionally depends on the host scheduler, so timing itself is not expected to be byte-for-byte identical.

## Tests

Unit tests:

```bash
./build/feed_unit_tests
```

Or:

```bash
ctest --test-dir build --output-on-failure
```

Regression test against `tests/golden/session.bin`:

```bash
./tests/run_regression.sh build
```

Integration test simulator -> UDP -> handler -> book -> capture -> replay:

```bash
./tests/run_integration.sh build
```

## Benchmark

One workload:

```bash
./build/feed_benchmark --messages 100000
```

Required workload sizes:

```bash
./build/feed_benchmark --messages 100000
./build/feed_benchmark --messages 1000000
./build/feed_benchmark --messages 10000000
```

Append results to the requested CSV:

```bash
./scripts/benchmark.sh
```

The repository starts with only the CSV header. No benchmark number is fabricated into source control. The benchmark reports `transport=in-memory`, so the numbers describe the processing pipeline rather than claiming UDP wire performance.

## Sequence validation

The expected sequence is tracked explicitly. `N -> N` is normal. `N -> N+5` increments the gap counter and accepts the received event as the next observed sequence. A repeat of the last received sequence is a duplicate. A lower, different sequence is out of order. Duplicates and out-of-order events are recorded but not applied to the book.

## Local order book

The book maintains independent bid and ask trees for each instrument. `top()` reports best bid and ask. Output can be top-1, top-5, or top-10. `REPLACE_ORDER` changes price/quantity while preserving `order_id`; `CANCEL_ORDER` and `EXECUTE_ORDER` reduce live quantity.

## Latency measurement

The live path records source, receive, and processing timestamps using a Linux monotonic clock. Wall-clock time is unsuitable for elapsed-latency measurement because it can be adjusted; monotonic time is not a guarantee of nanosecond accuracy. See `docs/performance.md` for clock, profiling, and allocation details.

## Linux profiling

```bash
perf stat -e cycles,instructions,branches,branch-misses,cache-misses,context-switches \
  ./build/feed_benchmark --messages 1000000

perf record -g ./build/feed_benchmark --messages 1000000
perf report

strace -f -tt -T -e trace=network,epoll_wait,epoll_ctl,read,write \
  ./build/feed_handler --port 9000 --count 1000
```

`perf`/`strace` alter timing and are for investigation, not low-latency production measurement.

## Limitations

* The wire protocol is custom and intentionally educational.
* The UDP path is the standard Linux kernel networking stack, not kernel-bypass.
* The order book uses STL trees/hash tables and can allocate.
* CSV output and recording introduce I/O costs when enabled.
* Source and host clocks are not assumed synchronized.
* No production exchange connectivity, multicast feed, recovery channel, or failover mechanism is implemented.
* DPDK is not implemented or required.

## Future direction

The architecture is intentionally transport-neutral so a future implementation can add multicast, `recvmmsg()` batching, CPU affinity, NUMA placement, zero-copy techniques, cache-aware data structures, lock-free pipeline refinements, a real DPDK adapter, and higher-throughput fixed-layout parsing. See the December evolution roadmap in `docs/performance.md`.

## Acceptance checklist

The supplied project includes a clean CMake build, unit tests, simulator, UDP/epoll feed handler, decoder validation, sequence-gap detection, multi-instrument book maintenance, capture/replay, golden regression testing, benchmark code, release-compatible scripts, documentation, and no required DPDK functionality hidden behind a fake implementation.
