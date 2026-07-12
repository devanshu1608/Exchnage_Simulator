#pragma once

/// @file raw_message.hpp
/// @brief Fixed-size raw message envelope for the SPSC queue.
///
/// The epoll thread frames TCP messages and pushes raw bytes here.
/// The matching thread pops and parses them.

#include <types.hpp>
#include <cstdint>

namespace exchange {

/// Maximum wire message size (largest current msg is ~40 bytes, generous headroom)
static constexpr std::size_t MAX_RAW_MSG_SIZE = 128;

struct alignas(CACHE_LINE_SIZE) RawMessage {
    TraderId traderId{0};
    uint16_t length{0};
    char     data[MAX_RAW_MSG_SIZE]{};
};

static_assert(std::is_trivially_copyable_v<RawMessage>,
    "RawMessage must be trivially copyable for lock-free queues");

} // namespace exchange
