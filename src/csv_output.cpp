#include "csv_output.hpp"

namespace md {

bool SnapshotWriter::open(const std::string& path, std::size_t depth, std::string& error) {
    output_.open(path, std::ios::trunc);
    if (!output_) {
        error = "cannot open output: " + path;
        return false;
    }
    depth_ = depth == 0 ? 1 : depth;
    if (depth_ == 1) {
        output_ << "timestamp_ns,instrument,bid_price,bid_qty,ask_price,ask_qty\n";
    } else {
        output_ << "timestamp_ns,instrument,depth,bids,asks\n";
    }
    return true;
}

bool SnapshotWriter::open_latency(const std::string& path, std::string& error) {
    latency_.open(path, std::ios::trunc);
    if (!latency_) {
        error = "cannot open latency output: " + path;
        return false;
    }
    latency_ << "source_timestamp_ns,receive_timestamp_ns,decode_timestamp_ns,processing_timestamp_ns,processing_end_timestamp_ns,network_receive_delta,decode_delta,processing_delta,end_to_end_delta\n";
    return true;
}

void SnapshotWriter::close() noexcept {
    if (output_.is_open()) output_.close();
    if (latency_.is_open()) latency_.close();
}

void SnapshotWriter::write(const LocalOrderBook& book, std::uint64_t timestamp_ns,
                           std::uint32_t instrument_id) {
    if (!output_) return;
    if (depth_ == 1) book.write_snapshot(output_, timestamp_ns, instrument_id);
    else book.write_depth(output_, timestamp_ns, instrument_id, depth_);
}

void SnapshotWriter::write_latency(std::uint64_t source_timestamp_ns,
                                   std::uint64_t receive_timestamp_ns,
                                   std::uint64_t decode_timestamp_ns,
                                   std::uint64_t processing_timestamp_ns,
                                   std::uint64_t processing_end_timestamp_ns) {
    if (!latency_.is_open()) return;
    const auto network_receive_delta = receive_timestamp_ns >= source_timestamp_ns
        ? receive_timestamp_ns - source_timestamp_ns : 0;
    const auto decode_delta = decode_timestamp_ns >= receive_timestamp_ns
        ? decode_timestamp_ns - receive_timestamp_ns : 0;
    const auto processing_delta = processing_end_timestamp_ns >= processing_timestamp_ns
        ? processing_end_timestamp_ns - processing_timestamp_ns : 0;
    const auto end_to_end_delta = processing_end_timestamp_ns >= source_timestamp_ns
        ? processing_end_timestamp_ns - source_timestamp_ns : 0;
    latency_ << source_timestamp_ns << ',' << receive_timestamp_ns << ','
             << decode_timestamp_ns << ',' << processing_timestamp_ns << ','
             << processing_end_timestamp_ns << ',' << network_receive_delta << ','
             << decode_delta << ',' << processing_delta << ',' << end_to_end_delta << '\n';
}

} // namespace md
