#pragma once

#include "protocol.hpp"
#include <cstdint>

namespace md {

struct SequenceCounters {
    std::uint64_t gaps_detected{0};
    std::uint64_t duplicates_detected{0};
    std::uint64_t out_of_order_messages{0};
};

class SequenceValidator {
public:
    explicit SequenceValidator(std::uint64_t initial_expected = 1) noexcept;

    SequenceEvent observe(std::uint64_t sequence) noexcept;
    std::uint64_t expected() const noexcept { return expected_sequence_; }
    const SequenceCounters& counters() const noexcept { return counters_; }
    void reset(std::uint64_t expected = 1) noexcept;

private:
    std::uint64_t expected_sequence_;
    std::uint64_t last_received_;
    bool have_last_{false};
    SequenceCounters counters_{};
};

} // namespace md
