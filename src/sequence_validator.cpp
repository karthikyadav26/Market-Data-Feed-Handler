#include "sequence_validator.hpp"

namespace md {

SequenceValidator::SequenceValidator(std::uint64_t initial_expected) noexcept
    : expected_sequence_(initial_expected), last_received_(0) {}

void SequenceValidator::reset(std::uint64_t expected) noexcept {
    expected_sequence_ = expected;
    last_received_ = 0;
    have_last_ = false;
    counters_ = {};
}

SequenceEvent SequenceValidator::observe(std::uint64_t sequence) noexcept {
    SequenceEvent result = SequenceEvent::NORMAL;
    if (!have_last_) {
        if (sequence > expected_sequence_) {
            result = SequenceEvent::SEQUENCE_GAP;
            ++counters_.gaps_detected;
        } else if (sequence < expected_sequence_) {
            result = SequenceEvent::OUT_OF_ORDER;
            ++counters_.out_of_order_messages;
        }
    } else if (sequence == expected_sequence_) {
        result = SequenceEvent::NORMAL;
    } else if (sequence > expected_sequence_) {
        result = SequenceEvent::SEQUENCE_GAP;
        ++counters_.gaps_detected;
    } else if (sequence == last_received_) {
        result = SequenceEvent::DUPLICATE;
        ++counters_.duplicates_detected;
    } else {
        result = SequenceEvent::OUT_OF_ORDER;
        ++counters_.out_of_order_messages;
    }

    last_received_ = sequence;
    have_last_ = true;
    if (sequence >= expected_sequence_) {
        expected_sequence_ = sequence + 1;
    }
    return result;
}

} // namespace md
