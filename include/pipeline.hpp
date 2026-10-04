#pragma once

#include "decoder.hpp"
#include "local_order_book.hpp"
#include "recorder.hpp"
#include "sequence_validator.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace md {

struct FeedEvent {
    MarketDataMessage message{};
    std::uint64_t receive_timestamp_ns{0};
    std::uint64_t decode_timestamp_ns{0};
};

struct FeedMetrics {
    std::atomic<std::uint64_t> messages_received{0};
    std::atomic<std::uint64_t> messages_decoded{0};
    std::atomic<std::uint64_t> messages_dropped{0};
    std::atomic<std::uint64_t> gaps_detected{0};
    std::atomic<std::uint64_t> duplicates_detected{0};
    std::atomic<std::uint64_t> out_of_order_messages{0};
    std::atomic<std::uint64_t> decode_errors{0};
    std::atomic<std::uint64_t> queue_drops{0};
    std::atomic<std::uint64_t> book_updates_failed{0};
};

struct ProcessorConfig {
    std::size_t output_depth{1};
};

class FeedProcessor {
public:
    FeedProcessor(LocalOrderBook& book, SequenceValidator& sequence,
                  FeedMetrics& metrics, FeedRecorder* recorder,
                  class SnapshotWriter* writer)
        : book_(book), sequence_(sequence), metrics_(metrics), recorder_(recorder), writer_(writer) {}

    bool handle_raw(std::span<const std::uint8_t> raw, std::uint64_t receive_timestamp_ns,
                    std::size_t& accepted_count) noexcept;
    bool handle_event(const FeedEvent& event) noexcept;

private:
    LocalOrderBook& book_;
    SequenceValidator& sequence_;
    FeedMetrics& metrics_;
    FeedRecorder* recorder_{nullptr};
    class SnapshotWriter* writer_{nullptr};
};

} // namespace md
