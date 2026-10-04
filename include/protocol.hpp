#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace md {

inline constexpr std::uint8_t kProtocolVersion = 1;
inline constexpr std::uint16_t kWireMessageSize = 48;
inline constexpr std::size_t kMaxDatagramSize = 64 * 1024;

// Exact on-wire layout; the C++ object is NOT serialized by memcpy.
// 0  : message_length     u16
// 2  : protocol_version   u8
// 3  : message_type       u8
// 4  : sequence_number    u64
// 12 : source_timestamp   u64
// 20 : instrument_id      u32
// 24 : order_id            u64
// 32 : side                u8
// 33 : reserved            u8[3]
// 36 : price_ticks         i64
// 44 : quantity             u32

enum class MessageType : std::uint8_t {
    ADD_ORDER = 1,
    CANCEL_ORDER = 2,
    EXECUTE_ORDER = 3,
    REPLACE_ORDER = 4,
};

enum class Side : std::uint8_t {
    BUY = 1,
    SELL = 2,
};

enum class SequenceEvent : std::uint8_t {
    NORMAL = 0,
    SEQUENCE_GAP = 1,
    DUPLICATE = 2,
    OUT_OF_ORDER = 3,
};

struct MarketDataMessage {
    std::uint8_t protocol_version{0};
    MessageType type{MessageType::ADD_ORDER};
    std::uint64_t sequence_number{0};
    std::uint64_t source_timestamp_ns{0};
    std::uint32_t instrument_id{0};
    std::uint64_t order_id{0};
    Side side{Side::BUY};
    std::int64_t price_ticks{0};
    std::uint32_t quantity{0};
};

constexpr bool is_valid_message_type(std::uint8_t value) noexcept {
    return value >= static_cast<std::uint8_t>(MessageType::ADD_ORDER) &&
           value <= static_cast<std::uint8_t>(MessageType::REPLACE_ORDER);
}

constexpr bool is_valid_side(std::uint8_t value) noexcept {
    return value == static_cast<std::uint8_t>(Side::BUY) ||
           value == static_cast<std::uint8_t>(Side::SELL);
}

constexpr std::uint16_t read_be16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8) |
                                      static_cast<std::uint16_t>(p[1]));
}

constexpr std::uint32_t read_be32(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

constexpr std::uint64_t read_be64(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint64_t>(p[0]) << 56) |
           (static_cast<std::uint64_t>(p[1]) << 48) |
           (static_cast<std::uint64_t>(p[2]) << 40) |
           (static_cast<std::uint64_t>(p[3]) << 32) |
           (static_cast<std::uint64_t>(p[4]) << 24) |
           (static_cast<std::uint64_t>(p[5]) << 16) |
           (static_cast<std::uint64_t>(p[6]) << 8) |
           static_cast<std::uint64_t>(p[7]);
}

constexpr void write_be16(std::uint8_t* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v >> 8);
    p[1] = static_cast<std::uint8_t>(v);
}

constexpr void write_be32(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v >> 24);
    p[1] = static_cast<std::uint8_t>(v >> 16);
    p[2] = static_cast<std::uint8_t>(v >> 8);
    p[3] = static_cast<std::uint8_t>(v);
}

constexpr void write_be64(std::uint8_t* p, std::uint64_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v >> 56);
    p[1] = static_cast<std::uint8_t>(v >> 48);
    p[2] = static_cast<std::uint8_t>(v >> 40);
    p[3] = static_cast<std::uint8_t>(v >> 32);
    p[4] = static_cast<std::uint8_t>(v >> 24);
    p[5] = static_cast<std::uint8_t>(v >> 16);
    p[6] = static_cast<std::uint8_t>(v >> 8);
    p[7] = static_cast<std::uint8_t>(v);
}

using WireBuffer = std::array<std::uint8_t, kWireMessageSize>;

} // namespace md
