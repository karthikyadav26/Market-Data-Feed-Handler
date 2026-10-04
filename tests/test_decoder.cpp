#include "test_registry.hpp"
#include "decoder.hpp"
#include "encoder.hpp"
md::MarketDataMessage valid(){md::MarketDataMessage m{};m.protocol_version=1;m.type=md::MessageType::ADD_ORDER;m.sequence_number=1;m.source_timestamp_ns=100;m.instrument_id=1;m.order_id=10;m.side=md::Side::BUY;m.price_ticks=100;m.quantity=5;return m;}
TEST(decoder_rejects_truncated){auto m=valid();md::WireBuffer w{};std::size_t n=0;REQUIRE(md::Encoder::encode(m,w,n));md::MarketDataMessage out{};md::DecodeError e;REQUIRE(!md::Decoder::decode(std::span<const std::uint8_t>(w.data(),20),out,e));REQUIRE(e==md::DecodeError::TRUNCATED);}
TEST(decoder_rejects_invalid_type){auto m=valid();md::WireBuffer w{};std::size_t n=0;REQUIRE(md::Encoder::encode(m,w,n));w[3]=99;md::MarketDataMessage out{};md::DecodeError e;REQUIRE(!md::Decoder::decode(w,out,e));REQUIRE(e==md::DecodeError::INVALID_TYPE);}
TEST(encoder_rejects_invalid_field){auto m=valid();m.quantity=0;md::WireBuffer w{};std::size_t n=0;REQUIRE(!md::Encoder::encode(m,w,n));}
