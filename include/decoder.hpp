#pragma once

#include "protocol.hpp"
#include <cstddef>
#include <span>
#include <string_view>

namespace md {

enum class DecodeError : std::uint8_t {
    NONE = 0,
    TRUNCATED,
    INVALID_LENGTH,
    INVALID_VERSION,
    INVALID_TYPE,
    INVALID_SIDE,
    INVALID_FIELD,
};

class Decoder {
public:
    static bool decode(std::span<const std::uint8_t> bytes,
                       MarketDataMessage& out,
                       DecodeError& error) noexcept;

    static constexpr std::string_view error_string(DecodeError error) noexcept {
        switch (error) {
            case DecodeError::NONE: return "none";
            case DecodeError::TRUNCATED: return "truncated";
            case DecodeError::INVALID_LENGTH: return "invalid_length";
            case DecodeError::INVALID_VERSION: return "invalid_version";
            case DecodeError::INVALID_TYPE: return "invalid_type";
            case DecodeError::INVALID_SIDE: return "invalid_side";
            case DecodeError::INVALID_FIELD: return "invalid_field";
        }
        return "unknown";
    }
};

} // namespace md
