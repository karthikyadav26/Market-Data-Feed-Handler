#pragma once

namespace md {

// Design placeholder only. This type intentionally has no implementation and
// is not compiled into the default build. A future adapter should implement
// IMarketDataTransport using DPDK mbufs/burst APIs while keeping decoder,
// sequence validation, SPSC queue, and book-processing code unchanged.
class DpdkTransport {
public:
    static constexpr bool kImplemented = false;
};

} // namespace md
