#include "test_registry.hpp"
#include "encoder.hpp"
#include "recorder.hpp"
#include "decoder.hpp"
#include <cstdio>
TEST(recorder_round_trip){const std::string path="/tmp/md_feed_replay_unit_capture.bin";md::FeedRecorder w;std::string err;REQUIRE(w.open(path,err));md::MarketDataMessage m{};m.protocol_version=1;m.type=md::MessageType::ADD_ORDER;m.sequence_number=1;m.source_timestamp_ns=123;m.instrument_id=1;m.order_id=1;m.side=md::Side::BUY;m.price_ticks=100;m.quantity=1;md::WireBuffer raw{};std::size_t n=0;REQUIRE(md::Encoder::encode(m,raw,n));REQUIRE(w.record(456,raw));w.close();md::FeedCaptureReader r;REQUIRE(r.open(path,err));std::uint64_t ts=0;std::span<const std::uint8_t> bytes;REQUIRE(r.next(ts,bytes));REQUIRE(ts==456);md::MarketDataMessage out{};md::DecodeError e;REQUIRE(md::Decoder::decode(bytes,out,e));REQUIRE(out.sequence_number==1);std::remove(path.c_str());}
