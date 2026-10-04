#pragma once

#include "protocol.hpp"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <fstream>
#include <memory>
#include <string>

namespace md {

class CsvFeedReader {
public:
    bool open(const std::string& path, std::string& error);
    bool next(MarketDataMessage& msg, std::string& error);

private:
    std::ifstream* input_ptr_ = nullptr;
    std::unique_ptr<std::ifstream> input_;
    std::size_t line_number_{0};
};

bool parse_message_type(const std::string& value, MessageType& type) noexcept;
bool parse_side(const std::string& value, Side& side) noexcept;
const char* message_type_name(MessageType type) noexcept;
const char* side_name(Side side) noexcept;

} // namespace md
