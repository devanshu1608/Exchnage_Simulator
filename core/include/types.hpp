#pragma once

/// @file types.hpp
/// @brief Core type aliases and enums for the exchange simulator.
///
/// Design principles:
///   - All types are POD / trivially copyable — no heap allocations.
///   - Prices use int64_t fixed-point (×10000) — never floating-point.
///   - Symbols are numeric IDs internally — string mapping at gateway only.
///   - TraderId is the single identity for a connected participant.

#include <cstdint>
#include <string_view>

namespace exchange {

// ─── Fundamental Types ───────────────────────────────────────────────

using Price = std::uint32_t;

using Quantity = std::uint32_t;

using OrderId = std::uint64_t;

using Timestamp = std::uint64_t;

using TraderId = std::uint32_t;

using SymbolId = std::uint32_t;

static constexpr std::size_t MAX_CLIENTS = 256;
static constexpr std::size_t MAX_INSTRUMENTS = 256;

static constexpr std::size_t CACHE_LINE_SIZE = 64;

// ─── Enumerations ────────────────────────────────────────────────────

/// Buy or Sell.
enum class Side : std::uint8_t {
    Buy  = 0,
    Sell = 1
};

/// Distinguishes request types in the internal queue.
enum class RequestType : std::uint8_t {
    New    = 0,
    Cancel = 1,
    Modify = 2
};

/// Reason for rejecting a request.
enum class RejectReason : std::uint8_t {
    UnknownOrder  = 0,
    InvalidPrice  = 1,
    InvalidQty    = 2,
    Unauthorized  = 3,
    UnknownSymbol = 4,
    SelfTrade     = 5
};

// ─── Utility Functions ───────────────────────────────────────────────

/// String representation of Side.
[[nodiscard]] constexpr std::string_view to_string(Side s) noexcept {
    return s == Side::Buy ? "BUY" : "SELL";
}

/// Opposite side helper.
[[nodiscard]] constexpr Side oppositeSide(Side s) noexcept {
    return s == Side::Buy ? Side::Sell : Side::Buy;
}

} // namespace exchange
