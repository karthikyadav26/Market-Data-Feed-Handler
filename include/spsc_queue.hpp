#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>

namespace md {

// Single-producer/single-consumer bounded queue.
// Producer owns head; consumer owns tail. Acquire/release pairs publish
// fully initialized elements without a lock in the steady state.
template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert(std::is_nothrow_copy_assignable_v<T>, "T must be nothrow copy assignable");

public:
    bool try_push(const T& value) noexcept {
        const std::size_t head = head_.value.load(std::memory_order_relaxed);
        const std::size_t next = increment(head);
        if (next == tail_.value.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[head] = value;
        head_.value.store(next, std::memory_order_release);
        return true;
    }

    bool try_push(T&& value) noexcept {
        const std::size_t head = head_.value.load(std::memory_order_relaxed);
        const std::size_t next = increment(head);
        if (next == tail_.value.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[head] = std::move(value);
        head_.value.store(next, std::memory_order_release);
        return true;
    }

    bool try_pop(T& out) noexcept {
        const std::size_t tail = tail_.value.load(std::memory_order_relaxed);
        if (tail == head_.value.load(std::memory_order_acquire)) {
            return false;
        }
        out = buffer_[tail];
        tail_.value.store(increment(tail), std::memory_order_release);
        return true;
    }

    bool empty() const noexcept {
        return tail_.value.load(std::memory_order_acquire) == head_.value.load(std::memory_order_acquire);
    }

private:
    static constexpr std::size_t increment(std::size_t value) noexcept {
        return (value + 1U) % Capacity;
    }

    struct alignas(64) PaddedAtomic {
        std::atomic<std::size_t> value{0};
    };

    std::unique_ptr<T[]> buffer_{new T[Capacity]};
    PaddedAtomic head_{};
    PaddedAtomic tail_{};
};

} // namespace md
