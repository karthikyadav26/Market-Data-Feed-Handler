#include "local_order_book.hpp"

#include <algorithm>
#include <iomanip>

namespace md {

namespace {

std::uint64_t safe_subtract(std::uint64_t current, std::uint32_t amount) noexcept {
    return current > amount ? current - amount : 0;
}

void write_level(std::ostream& out, const std::optional<BookLevel>& level) {
    if (level.has_value()) {
        out << level->price_ticks << ',' << level->quantity;
    } else {
        out << "0,0";
    }
}

} // namespace

LocalOrderBook::Book* LocalOrderBook::find_book(std::uint32_t instrument_id) noexcept {
    auto it = books_.find(instrument_id);
    return it == books_.end() ? nullptr : &it->second;
}

const LocalOrderBook::Book* LocalOrderBook::find_book(std::uint32_t instrument_id) const noexcept {
    auto it = books_.find(instrument_id);
    return it == books_.end() ? nullptr : &it->second;
}

void LocalOrderBook::add_level(Book& book, Side side, std::int64_t price, std::uint32_t qty) noexcept {
    if (side == Side::BUY) book.bids[price] += qty;
    else book.asks[price] += qty;
}

void LocalOrderBook::reduce_level(Book& book, Side side, std::int64_t price, std::uint32_t qty) noexcept {
    if (side == Side::BUY) {
        auto it = book.bids.find(price);
        if (it == book.bids.end()) return;
        it->second = safe_subtract(it->second, qty);
        if (it->second == 0) book.bids.erase(it);
    } else {
        auto it = book.asks.find(price);
        if (it == book.asks.end()) return;
        it->second = safe_subtract(it->second, qty);
        if (it->second == 0) book.asks.erase(it);
    }
}

bool LocalOrderBook::add(const MarketDataMessage& msg) noexcept {
    if (orders_.find(msg.order_id) != orders_.end()) {
        return false;
    }
    auto [book_it, inserted] = books_.try_emplace(msg.instrument_id);
    (void)inserted;
    add_level(book_it->second, msg.side, msg.price_ticks, msg.quantity);
    orders_.emplace(msg.order_id, OrderState{msg.instrument_id, msg.order_id, msg.side,
                                              msg.price_ticks, msg.quantity});
    return true;
}

bool LocalOrderBook::reduce(const MarketDataMessage& msg) noexcept {
    auto it = orders_.find(msg.order_id);
    if (it == orders_.end()) {
        return false;
    }
    auto& order = it->second;
    if (order.instrument_id != msg.instrument_id || order.side != msg.side) {
        return false;
    }
    const auto amount = std::min<std::uint32_t>(msg.quantity, order.quantity);
    auto* book = find_book(order.instrument_id);
    if (book == nullptr) {
        return false;
    }
    reduce_level(*book, order.side, order.price_ticks, amount);
    order.quantity -= amount;
    if (order.quantity == 0) {
        orders_.erase(it);
    }
    return true;
}

bool LocalOrderBook::replace(const MarketDataMessage& msg) noexcept {
    auto it = orders_.find(msg.order_id);
    if (it == orders_.end()) {
        return false;
    }
    auto& order = it->second;
    if (order.instrument_id != msg.instrument_id || order.side != msg.side) {
        return false;
    }
    auto* book = find_book(order.instrument_id);
    if (book == nullptr) {
        return false;
    }
    reduce_level(*book, order.side, order.price_ticks, order.quantity);
    add_level(*book, order.side, msg.price_ticks, msg.quantity);
    order.price_ticks = msg.price_ticks;
    order.quantity = msg.quantity;
    return true;
}

bool LocalOrderBook::apply(const MarketDataMessage& msg) noexcept {
    switch (msg.type) {
        case MessageType::ADD_ORDER: return add(msg);
        case MessageType::CANCEL_ORDER:
        case MessageType::EXECUTE_ORDER: return reduce(msg);
        case MessageType::REPLACE_ORDER: return replace(msg);
    }
    return false;
}

BookTop LocalOrderBook::top(std::uint32_t instrument_id) const noexcept {
    BookTop result{};
    const auto* book = find_book(instrument_id);
    if (book == nullptr) {
        return result;
    }
    if (!book->bids.empty()) {
        result.bid = BookLevel{book->bids.begin()->first, book->bids.begin()->second};
    }
    if (!book->asks.empty()) {
        result.ask = BookLevel{book->asks.begin()->first, book->asks.begin()->second};
    }
    return result;
}

std::vector<BookLevel> LocalOrderBook::bid_depth(std::uint32_t instrument_id, std::size_t levels) const {
    std::vector<BookLevel> result;
    result.reserve(levels);
    const auto* book = find_book(instrument_id);
    if (book == nullptr) return result;
    for (auto it = book->bids.begin(); it != book->bids.end() && result.size() < levels; ++it) {
        result.push_back({it->first, it->second});
    }
    return result;
}

std::vector<BookLevel> LocalOrderBook::ask_depth(std::uint32_t instrument_id, std::size_t levels) const {
    std::vector<BookLevel> result;
    result.reserve(levels);
    const auto* book = find_book(instrument_id);
    if (book == nullptr) return result;
    for (auto it = book->asks.begin(); it != book->asks.end() && result.size() < levels; ++it) {
        result.push_back({it->first, it->second});
    }
    return result;
}

void LocalOrderBook::write_snapshot(std::ostream& out, std::uint64_t timestamp_ns,
                                    std::uint32_t instrument_id) const {
    const BookTop current = top(instrument_id);
    out << timestamp_ns << ',' << instrument_id << ',';
    write_level(out, current.bid);
    out << ',';
    write_level(out, current.ask);
    out << '\n';
}

void LocalOrderBook::write_depth(std::ostream& out, std::uint64_t timestamp_ns,
                                 std::uint32_t instrument_id, std::size_t levels) const {
    const auto bids = bid_depth(instrument_id, levels);
    const auto asks = ask_depth(instrument_id, levels);
    out << timestamp_ns << ',' << instrument_id << ',' << levels << ',';
    for (std::size_t i = 0; i < levels; ++i) {
        if (i < bids.size()) out << bids[i].price_ticks << ':' << bids[i].quantity;
        else out << "0:0";
        if (i + 1 < levels) out << '|';
    }
    out << ',';
    for (std::size_t i = 0; i < levels; ++i) {
        if (i < asks.size()) out << asks[i].price_ticks << ':' << asks[i].quantity;
        else out << "0:0";
        if (i + 1 < levels) out << '|';
    }
    out << '\n';
}

} // namespace md
