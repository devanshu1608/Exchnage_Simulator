#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <type_traits>

namespace exchange {

/// Simple Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
/// Simplified version for V1.
template<typename T, std::size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity >= 2, "Capacity must be >= 2");
    
    static constexpr std::size_t MASK = Capacity - 1;

    // Align to prevent false sharing between producer and consumer threads
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) std::array<T, Capacity> buffer_{};

public:
    SPSCQueue() = default;

    /// Try to push an element. Returns false if full.
    bool try_push(const T& item) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next_head = (head + 1) & MASK;

        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; // Full
        }

        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    /// Try to pop an element. Returns false if empty.
    bool try_pop(T& item) {
        const auto tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // Empty
        }

        item = std::move(buffer_[tail]);
        tail_.store((tail + 1) & MASK, std::memory_order_release);
        return true;
    }

    // Blocking push
    void push(const T& item) {
        while (!try_push(item)) {
            // Spin
        }
    }

    // Blocking pop
    void pop(T& item) {
        while (!try_pop(item)) {
            // Spin
        }
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    bool full() const {
        const auto head = head_.load(std::memory_order_acquire);
        return ((head + 1) & MASK) == tail_.load(std::memory_order_acquire);
    }

    std::size_t size() const {
        return (head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire)) & MASK;
    }
    
    std::size_t capacity() const {
        return Capacity - 1;
    }
};

} // namespace exchange
