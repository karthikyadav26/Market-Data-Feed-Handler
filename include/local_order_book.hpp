#pragma once

#include "protocol.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <vector>

namespace md {

struct BookLevel {
    std::int64_t price_ticks{0};
    std::uint64_t quantity{0};
};

struct BookTop {
    std::optional<BookLevel> bid;
    std::optional<BookLevel> ask;
};

class LocalOrderBook {
public:
    bool apply(const MarketDataMessage& msg) noexcept;
    BookTop top(std::uint32_t instrument_id) const noexcept;
    std::vector<BookLevel> bid_depth(std::uint32_t instrument_id, std::size_t levels) const;
    std::vector<BookLevel> ask_depth(std::uint32_t instrument_id, std::size_t levels) const;
    void write_snapshot(std::ostream& out, std::uint64_t timestamp_ns,
                        std::uint32_t instrument_id) const;
    void write_depth(std::ostream& out, std::uint64_t timestamp_ns,
                     std::uint32_t instrument_id, std::size_t levels) const;
    std::size_t live_order_count() const noexcept { return orders_.size(); }

private:
    struct OrderState {
        std::uint32_t instrument_id{0};
        std::uint64_t order_id{0};
        Side side{Side::BUY};
        std::int64_t price_ticks{0};
        std::uint32_t quantity{0};
    };

    struct Book {
        std::map<std::int64_t, std::uint64_t, std::greater<std::int64_t>> bids;
        std::map<std::int64_t, std::uint64_t> asks;
    };

    bool add(const MarketDataMessage& msg) noexcept;
    bool reduce(const MarketDataMessage& msg) noexcept;
    bool replace(const MarketDataMessage& msg) noexcept;
    void add_level(Book& book, Side side, std::int64_t price, std::uint32_t qty) noexcept;
    void reduce_level(Book& book, Side side, std::int64_t price, std::uint32_t qty) noexcept;
    const Book* find_book(std::uint32_t instrument_id) const noexcept;
    Book* find_book(std::uint32_t instrument_id) noexcept;

    std::unordered_map<std::uint64_t, OrderState> orders_;
    std::unordered_map<std::uint32_t, Book> books_;
};

} // namespace md
