#include "test_registry.hpp"
#include "encoder.hpp"
#include "decoder.hpp"
TEST(protocol_round_trip){md::MarketDataMessage in{};in.protocol_version=1;in.type=md::MessageType::REPLACE_ORDER;in.sequence_number=0x0102030405060708ULL;in.source_timestamp_ns=1234567890;in.instrument_id=42;in.order_id=987654321;in.side=md::Side::SELL;in.price_ticks=10125;in.quantity=77;md::WireBuffer w{};std::size_t n=0;REQUIRE(md::Encoder::encode(in,w,n));REQUIRE_EQ(n,48U);md::MarketDataMessage out{};md::DecodeError e;REQUIRE(md::Decoder::decode(w,out,e));REQUIRE_EQ(out.sequence_number,in.sequence_number);REQUIRE_EQ(out.price_ticks,in.price_ticks);REQUIRE_EQ(out.quantity,in.quantity);}
TEST(protocol_byte_order){md::WireBuffer w{};md::write_be64(w.data(),0x0102030405060708ULL);REQUIRE_EQ(md::read_be64(w.data()),0x0102030405060708ULL);md::write_be32(w.data(),0xAABBCCDD);REQUIRE_EQ(md::read_be32(w.data()),0xAABBCCDDU);}
