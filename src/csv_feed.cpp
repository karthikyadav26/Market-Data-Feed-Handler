#include "csv_feed.hpp"

#include <charconv>
#include <fstream>
#include <memory>
#include <sstream>
#include <string_view>
#include <vector>

namespace md {

namespace {

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) fields.push_back(field);
    return fields;
}

template <typename T>
bool parse_integer(const std::string& s, T& out) {
    const char* begin = s.data();
    const char* end = begin + s.size();
    auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

} // namespace

bool parse_message_type(const std::string& value, MessageType& type) noexcept {
    if (value == "ADD_ORDER") { type = MessageType::ADD_ORDER; return true; }
    if (value == "CANCEL_ORDER") { type = MessageType::CANCEL_ORDER; return true; }
    if (value == "EXECUTE_ORDER") { type = MessageType::EXECUTE_ORDER; return true; }
    if (value == "REPLACE_ORDER") { type = MessageType::REPLACE_ORDER; return true; }
    return false;
}

bool parse_side(const std::string& value, Side& side) noexcept {
    if (value == "BUY") { side = Side::BUY; return true; }
    if (value == "SELL") { side = Side::SELL; return true; }
    return false;
}

const char* message_type_name(MessageType type) noexcept {
    switch (type) {
        case MessageType::ADD_ORDER: return "ADD_ORDER";
        case MessageType::CANCEL_ORDER: return "CANCEL_ORDER";
        case MessageType::EXECUTE_ORDER: return "EXECUTE_ORDER";
        case MessageType::REPLACE_ORDER: return "REPLACE_ORDER";
    }
    return "UNKNOWN";
}

const char* side_name(Side side) noexcept {
    return side == Side::BUY ? "BUY" : "SELL";
}

bool CsvFeedReader::open(const std::string& path, std::string& error) {
    input_ = std::make_unique<std::ifstream>(path);
    input_ptr_ = input_.get();
    if (!*input_ptr_) {
        error = "cannot open feed CSV: " + path;
        return false;
    }
    std::string header;
    if (!std::getline(*input_ptr_, header)) {
        error = "feed CSV is empty: " + path;
        return false;
    }
    line_number_ = 1;
    return true;
}

bool CsvFeedReader::next(MarketDataMessage& msg, std::string& error) {
    if (input_ptr_ == nullptr) {
        error = "CSV reader is not open";
        return false;
    }
    std::string line;
    if (!std::getline(*input_ptr_, line)) return false;
    ++line_number_;
    auto f = split_csv(line);
    if (f.size() != 8) {
        error = "line " + std::to_string(line_number_) + ": expected 8 CSV fields";
        return false;
    }
    if (!parse_integer(f[0], msg.sequence_number) ||
        !parse_message_type(f[1], msg.type) ||
        !parse_integer(f[2], msg.instrument_id) ||
        !parse_integer(f[3], msg.order_id) ||
        !parse_side(f[4], msg.side) ||
        !parse_integer(f[5], msg.price_ticks) ||
        !parse_integer(f[6], msg.quantity) ||
        !parse_integer(f[7], msg.source_timestamp_ns)) {
        error = "line " + std::to_string(line_number_) + ": invalid field";
        return false;
    }
    msg.protocol_version = kProtocolVersion;
    if (msg.instrument_id == 0 || msg.order_id == 0 || msg.price_ticks <= 0 || msg.quantity == 0) {
        error = "line " + std::to_string(line_number_) + ": invalid field value";
        return false;
    }
    return true;
}

} // namespace md
