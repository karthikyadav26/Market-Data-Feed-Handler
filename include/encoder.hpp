#pragma once

#include "protocol.hpp"
#include <cstddef>
#include <span>

namespace md {

class Encoder {
public:
    static bool encode(const MarketDataMessage& msg, std::span<std::uint8_t> out,
                       std::size_t& written) noexcept;
};

} // namespace md
