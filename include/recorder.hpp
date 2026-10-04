#pragma once

#include <cstddef>
#include <array>
#include <array>
#include <cstdint>
#include <fstream>
#include <span>
#include <string>

namespace md {

inline constexpr std::uint16_t kCaptureVersion = 1;

class FeedRecorder {
public:
    FeedRecorder() = default;
    ~FeedRecorder();

    bool open(const std::string& path, std::string& error);
    bool record(std::uint64_t receive_timestamp_ns,
                std::span<const std::uint8_t> original_message) noexcept;
    void close() noexcept;
    bool good() const noexcept { return out_.good(); }

private:
    std::ofstream out_;
};

class FeedCaptureReader {
public:
    bool open(const std::string& path, std::string& error);
    bool next(std::uint64_t& receive_timestamp_ns,
              std::span<const std::uint8_t>& original_message) noexcept;

private:
    static constexpr std::size_t kBufferSize = 64 * 1024;
    std::ifstream in_;
    std::uint32_t pending_length_{0};
    std::array<std::uint8_t, kBufferSize> buffer_{};
};

} // namespace md
