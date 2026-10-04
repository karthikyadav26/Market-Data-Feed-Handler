#include "decoder.hpp"

namespace md {

bool Decoder::decode(std::span<const std::uint8_t> bytes,
                     MarketDataMessage& out,
                     DecodeError& error) noexcept {
    error = DecodeError::NONE;
    if (bytes.size() < 2) {
        error = DecodeError::TRUNCATED;
        return false;
    }
    const std::uint16_t length = read_be16(bytes.data());
    if (length != kWireMessageSize) {
        error = DecodeError::INVALID_LENGTH;
        return false;
    }
    if (bytes.size() < kWireMessageSize) {
        error = DecodeError::TRUNCATED;
        return false;
    }
    if (bytes.size() != kWireMessageSize) {
        error = DecodeError::INVALID_LENGTH;
        return false;
    }

    const auto version = bytes[2];
    const auto type = bytes[3];
    const auto side = bytes[32];
    if (version != kProtocolVersion) {
        error = DecodeError::INVALID_VERSION;
        return false;
    }
    if (!is_valid_message_type(type)) {
        error = DecodeError::INVALID_TYPE;
        return false;
    }
    if (!is_valid_side(side)) {
        error = DecodeError::INVALID_SIDE;
        return false;
    }

    out.protocol_version = version;
    out.type = static_cast<MessageType>(type);
    out.sequence_number = read_be64(bytes.data() + 4);
    out.source_timestamp_ns = read_be64(bytes.data() + 12);
    out.instrument_id = read_be32(bytes.data() + 20);
    out.order_id = read_be64(bytes.data() + 24);
    out.side = static_cast<Side>(side);
    out.price_ticks = static_cast<std::int64_t>(read_be64(bytes.data() + 36));
    out.quantity = read_be32(bytes.data() + 44);

    if (out.instrument_id == 0 || out.order_id == 0 || out.price_ticks <= 0 || out.quantity == 0) {
        error = DecodeError::INVALID_FIELD;
        return false;
    }
    return true;
}

} // namespace md
