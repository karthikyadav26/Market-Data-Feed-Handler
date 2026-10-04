#include "encoder.hpp"

namespace md {

bool Encoder::encode(const MarketDataMessage& msg, std::span<std::uint8_t> out,
                     std::size_t& written) noexcept {
    written = 0;
    if (out.size() < kWireMessageSize) {
        return false;
    }
    if (msg.protocol_version != kProtocolVersion ||
        !is_valid_message_type(static_cast<std::uint8_t>(msg.type)) ||
        !is_valid_side(static_cast<std::uint8_t>(msg.side)) ||
        msg.instrument_id == 0 || msg.order_id == 0 || msg.price_ticks <= 0 || msg.quantity == 0) {
        return false;
    }

    write_be16(out.data() + 0, kWireMessageSize);
    out[2] = msg.protocol_version;
    out[3] = static_cast<std::uint8_t>(msg.type);
    write_be64(out.data() + 4, msg.sequence_number);
    write_be64(out.data() + 12, msg.source_timestamp_ns);
    write_be32(out.data() + 20, msg.instrument_id);
    write_be64(out.data() + 24, msg.order_id);
    out[32] = static_cast<std::uint8_t>(msg.side);
    out[33] = out[34] = out[35] = 0;
    write_be64(out.data() + 36, static_cast<std::uint64_t>(msg.price_ticks));
    write_be32(out.data() + 44, msg.quantity);
    written = kWireMessageSize;
    return true;
}

} // namespace md
