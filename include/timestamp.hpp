#pragma once

#include <cstdint>
#include <time.h>

namespace md {

inline std::uint64_t monotonic_now_ns() noexcept {
    ::timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
           static_cast<std::uint64_t>(ts.tv_nsec);
}

} // namespace md
