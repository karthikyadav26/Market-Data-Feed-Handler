# Architecture

## Data path

```text
+------------------------+
| Market Data Simulator  |
| deterministic encoder  |
+-----------+------------+
            |
            | UDP localhost
            v
+------------------------+
| UdpTransport           |
| non-blocking socket    |
| SO_RCVBUF configurable |
| epoll readiness wait   |
+-----------+------------+
            |
            v
+------------------------+
| Binary Decoder         |
| fixed 48-byte messages |
+-----------+------------+
            |
            v
+------------------------+
| Sequence Validator     |
| normal/gap/duplicate   |
| out-of-order counters  |
+-----------+------------+
            |
            | MODE B
            v
+------------------------+
| SPSC Queue             |
| bounded, atomic        |
+-----------+------------+
            |
            v
+------------------------+
| Local Order Book       |
| per instrument         |
| bid/ask depth          |
+-----------+------------+
            |
            +---------> recorder
            |
            +---------> CSV consumer
```

MODE A combines transport, decode, sequence validation, and book processing on one thread. MODE B places transport/decode/sequence handling on a receiver thread and uses a bounded SPSC ring to hand accepted events to the book thread.

## Why the transport is isolated

`IMarketDataTransport` exposes lifecycle, readiness, and byte reception, while business logic only sees decoded fixed-size events. The default `UdpTransport` uses ordinary Linux UDP and epoll. A future DPDK adapter can satisfy the same transport contract with burst reception and a polling/wait strategy; no decoder or order-book rewrite is required.

The shipped `DpdkTransport` type is a design placeholder only and deliberately contains no fake DPDK code. CMake rejects `-DENABLE_DPDK=ON` instead of pretending DPDK exists.

## Linux networking path

The UDP implementation uses:

1. `socket(AF_INET, SOCK_DGRAM, 0)` to create the socket.
2. `fcntl(..., O_NONBLOCK)` so receive never blocks the processing loop.
3. `setsockopt(SO_RCVBUF)` for a configurable kernel receive buffer.
4. `bind()` on the requested local IPv4 address/port.
5. `epoll_create1()` and `epoll_ctl()` to register readable/error events.
6. `epoll_wait()` to sleep until data arrives.
7. `recv()` to drain datagrams until `EAGAIN`/`EWOULDBLOCK`.

This path still crosses the normal Linux kernel networking stack. It is not kernel-bypass. Kernel-bypass technologies such as DPDK can reduce per-packet kernel work, but they introduce different deployment and memory-management requirements.

## Threading and memory ordering

The SPSC queue uses one producer-owned head index and one consumer-owned tail index. The producer:

* reads its own head with relaxed ordering,
* checks the consumer tail with acquire,
* copies the event into the slot,
* publishes the new head with release.

The consumer:

* reads its own tail with relaxed ordering,
* checks the producer head with acquire,
* copies the slot to local storage,
* advances the tail with release.

The release/acquire pairs ensure initialized queue elements become visible to the other thread before ownership moves. Head and tail are separated onto cache-line-aligned storage to reduce false sharing.

The queue is bounded: when full, the receiver applies backpressure and yields until space is available unless shutdown is requested. The `queue_drops` metric remains exposed for deployments that choose an explicit drop policy; the shipped handler does not silently discard queued market-data events.

The order book intentionally uses standard library associative containers because this is an educational stack. These containers can allocate and cause pointer chasing. The code reserves/reuses fixed-size wire buffers and queue slots, but the order book itself is not a zero-allocation production design.

## Order book semantics

Each live order stores instrument, side, price, and remaining quantity. Aggregated price levels are held separately for bids and asks. Bids are ordered descending, asks ascending. `top()` therefore reads best bid and best ask from the beginning of each tree.

REPLACE_ORDER removes the old level contribution then adds the new price/quantity contribution. CANCEL_ORDER and EXECUTE_ORDER reduce quantity at the existing order's price.

## Recording and replay

The recorder is placed after sequence validation and before/alongside book processing. It captures the original bytes plus the receive timestamp, rather than storing only book state. Replay decodes the original messages and re-runs the same logical book update rules, enabling exact regression comparison of the emitted logical output.

Realtime replay uses recorded receive timestamps only for pacing. Max-speed replay ignores sleeps. Neither mode makes wall-clock timing part of the logical book state.

## Determinism boundary

Logical output is based on the recorded event sequence and source timestamp. Runtime receive and processing timestamps are observability fields and can vary by execution. Therefore regression compares the deterministic snapshot CSV, not runtime latency timestamps.
