#include "pipeline.hpp"
#include "csv_output.hpp"
#include "timestamp.hpp"

namespace md {

bool FeedProcessor::handle_raw(std::span<const std::uint8_t> raw,
                               std::uint64_t receive_timestamp_ns,
                               std::size_t& accepted_count) noexcept {
    accepted_count = 0;
    std::size_t cursor = 0;
    bool decode_failed = false;
    while (cursor < raw.size()) {
        const std::size_t remaining = raw.size() - cursor;
        const std::size_t chunk = remaining < kWireMessageSize ? remaining : kWireMessageSize;
        metrics_.messages_received.fetch_add(1, std::memory_order_relaxed);
        MarketDataMessage msg{};
        DecodeError err = DecodeError::NONE;
        if (!Decoder::decode(raw.subspan(cursor, chunk), msg, err)) {
            metrics_.decode_errors.fetch_add(1, std::memory_order_relaxed);
            metrics_.messages_dropped.fetch_add(1, std::memory_order_relaxed);
            decode_failed = true;
            cursor = raw.size();
            break;
        }
        metrics_.messages_decoded.fetch_add(1, std::memory_order_relaxed);
        const auto decode_timestamp_ns = monotonic_now_ns();
        const auto seq = sequence_.observe(msg.sequence_number);
        switch (seq) {
            case SequenceEvent::SEQUENCE_GAP: metrics_.gaps_detected.fetch_add(1, std::memory_order_relaxed); break;
            case SequenceEvent::DUPLICATE: metrics_.duplicates_detected.fetch_add(1, std::memory_order_relaxed); break;
            case SequenceEvent::OUT_OF_ORDER: metrics_.out_of_order_messages.fetch_add(1, std::memory_order_relaxed); break;
            case SequenceEvent::NORMAL: break;
        }
        if (recorder_ != nullptr) recorder_->record(receive_timestamp_ns, raw.subspan(cursor, kWireMessageSize));
        if (seq == SequenceEvent::DUPLICATE || seq == SequenceEvent::OUT_OF_ORDER) {
            metrics_.messages_dropped.fetch_add(1, std::memory_order_relaxed);
        } else {
            FeedEvent ev{msg, receive_timestamp_ns, decode_timestamp_ns};
            if (!handle_event(ev)) metrics_.messages_dropped.fetch_add(1, std::memory_order_relaxed);
            else ++accepted_count;
        }
        cursor += kWireMessageSize;
    }
    if (cursor < raw.size() && !decode_failed) metrics_.messages_dropped.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool FeedProcessor::handle_event(const FeedEvent& event) noexcept {
    const auto processing_timestamp_ns = monotonic_now_ns();
    const auto ok = book_.apply(event.message);
    const auto processing_end_timestamp_ns = monotonic_now_ns();
    if (!ok) {
        metrics_.book_updates_failed.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (writer_ != nullptr) {
        writer_->write(book_, event.message.source_timestamp_ns, event.message.instrument_id);
        writer_->write_latency(event.message.source_timestamp_ns,
                               event.receive_timestamp_ns,
                               event.decode_timestamp_ns,
                               processing_timestamp_ns,
                               processing_end_timestamp_ns);
    }
    return true;
}

} // namespace md
