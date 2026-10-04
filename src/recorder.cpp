#include "recorder.hpp"

#include "protocol.hpp"

#include <array>
#include <cstring>

namespace md {

namespace {
constexpr char kMagic[] = "MDCAP01";
constexpr std::size_t kPostMagicHeaderSize = 16;

void write_u16(char* p, std::uint16_t v) noexcept {
    auto* b = reinterpret_cast<std::uint8_t*>(p);
    write_be16(b, v);
}
void write_u32(char* p, std::uint32_t v) noexcept {
    auto* b = reinterpret_cast<std::uint8_t*>(p);
    write_be32(b, v);
}
void write_u64(char* p, std::uint64_t v) noexcept {
    auto* b = reinterpret_cast<std::uint8_t*>(p);
    write_be64(b, v);
}

std::uint16_t read_u16(const std::uint8_t* p) noexcept { return read_be16(p); }
std::uint32_t read_u32(const std::uint8_t* p) noexcept { return read_be32(p); }
std::uint64_t read_u64(const std::uint8_t* p) noexcept { return read_be64(p); }
}

FeedRecorder::~FeedRecorder() { close(); }

bool FeedRecorder::open(const std::string& path, std::string& error) {
    close();
    out_.open(path, std::ios::binary | std::ios::trunc);
    if (!out_) {
        error = "cannot open capture for write: " + path;
        return false;
    }
    out_.write(kMagic, sizeof(kMagic) - 1);
    std::array<char, 16> hdr{};
    write_u16(hdr.data(), kCaptureVersion);
    write_u16(hdr.data() + 2, 0);
    write_u32(hdr.data() + 4, 0);
    write_u64(hdr.data() + 8, 0);
    out_.write(hdr.data(), static_cast<std::streamsize>(hdr.size()));
    if (!out_) {
        error = "cannot write capture header";
        close();
        return false;
    }
    return true;
}

bool FeedRecorder::record(std::uint64_t receive_timestamp_ns,
                          std::span<const std::uint8_t> original_message) noexcept {
    if (!out_ || original_message.size() > kMaxDatagramSize) return false;
    std::array<char, kPostMagicHeaderSize> hdr{};
    write_u16(hdr.data(), kCaptureVersion);
    write_u16(hdr.data() + 2, 0);
    write_u32(hdr.data() + 4, static_cast<std::uint32_t>(original_message.size()));
    write_u64(hdr.data() + 8, receive_timestamp_ns);
    out_.write(hdr.data(), static_cast<std::streamsize>(hdr.size()));
    out_.write(reinterpret_cast<const char*>(original_message.data()),
               static_cast<std::streamsize>(original_message.size()));
    return static_cast<bool>(out_);
}

void FeedRecorder::close() noexcept {
    if (out_.is_open()) out_.close();
}

bool FeedCaptureReader::open(const std::string& path, std::string& error) {
    in_.open(path, std::ios::binary);
    if (!in_) {
        error = "cannot open capture: " + path;
        return false;
    }
    char magic[sizeof(kMagic) - 1]{};
    in_.read(magic, static_cast<std::streamsize>(sizeof(magic)));
    if (in_.gcount() != static_cast<std::streamsize>(sizeof(magic)) ||
        std::memcmp(magic, kMagic, sizeof(magic)) != 0) {
        error = "invalid capture magic";
        return false;
    }
    std::array<std::uint8_t, 16> hdr{};
    in_.read(reinterpret_cast<char*>(hdr.data()), static_cast<std::streamsize>(hdr.size()));
    if (in_.gcount() != static_cast<std::streamsize>(hdr.size()) ||
        read_u16(hdr.data()) != kCaptureVersion) {
        error = "unsupported capture header";
        return false;
    }
    return true;
}

bool FeedCaptureReader::next(std::uint64_t& receive_timestamp_ns,
                             std::span<const std::uint8_t>& original_message) noexcept {
    if (!in_) return false;
    std::array<std::uint8_t, 16> hdr{};
    in_.read(reinterpret_cast<char*>(hdr.data()), static_cast<std::streamsize>(hdr.size()));
    if (in_.gcount() == 0 && in_.eof()) return false;
    if (in_.gcount() != static_cast<std::streamsize>(hdr.size())) return false;

    pending_length_ = read_u32(hdr.data() + 4);
    receive_timestamp_ns = read_u64(hdr.data() + 8);
    if (pending_length_ == 0 || pending_length_ > buffer_.size()) return false;
    in_.read(reinterpret_cast<char*>(buffer_.data()), pending_length_);
    if (in_.gcount() != static_cast<std::streamsize>(pending_length_)) return false;
    original_message = std::span<const std::uint8_t>(buffer_.data(), pending_length_);
    return true;
}

} // namespace md
