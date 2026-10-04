#pragma once

#include "local_order_book.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

namespace md {

class SnapshotWriter {
public:
    bool open(const std::string& path, std::size_t depth, std::string& error);
    void close() noexcept;
    void write(const LocalOrderBook& book, std::uint64_t timestamp_ns,
               std::uint32_t instrument_id);
    bool open_latency(const std::string& path, std::string& error);
    void write_latency(std::uint64_t source_timestamp_ns,
                       std::uint64_t receive_timestamp_ns,
                       std::uint64_t decode_timestamp_ns,
                       std::uint64_t processing_timestamp_ns,
                       std::uint64_t processing_end_timestamp_ns);
    bool good() const noexcept { return output_.good(); }

private:
    std::ofstream output_;
    std::ofstream latency_;
    std::size_t depth_{1};
};

} // namespace md
