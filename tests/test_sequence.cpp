#include "test_registry.hpp"
#include "sequence_validator.hpp"
TEST(sequence_normal_gap_duplicate_ooo){md::SequenceValidator s(1);REQUIRE(s.observe(1)==md::SequenceEvent::NORMAL);REQUIRE(s.observe(2)==md::SequenceEvent::NORMAL);REQUIRE(s.observe(5)==md::SequenceEvent::SEQUENCE_GAP);REQUIRE(s.observe(5)==md::SequenceEvent::DUPLICATE);REQUIRE(s.observe(4)==md::SequenceEvent::OUT_OF_ORDER);REQUIRE_EQ(s.counters().gaps_detected,1U);REQUIRE_EQ(s.counters().duplicates_detected,1U);REQUIRE_EQ(s.counters().out_of_order_messages,1U);}
