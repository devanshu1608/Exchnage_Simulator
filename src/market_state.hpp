#pragma once

/// @file market_state.hpp
/// @brief Tracks current market state per instrument.
///
/// Updated by the matching thread after each trade.
/// Read by the HTTP server thread to serve the web GUI.

#include <types.hpp>

#include <array>
#include <mutex>

namespace exchange {

/// Current state of a single instrument.
struct InstrumentState {
    Price     bestBid{0};
    Price     bestAsk{0};
    Quantity  bidVolume{0};
    Quantity  askVolume{0};
    Price     lastTradePrice{0};
    Quantity  lastTradeQty{0};
    Quantity  totalTradedVolume{0};
};

/// Thread-safe market state container.
class MarketState {
public:
    /// Update state for a single instrument. Called by matching thread.
    void update(SymbolId symbol, const InstrumentState& state) {
        std::lock_guard<std::mutex> lock(mtx);
        if (symbol < MAX_INSTRUMENTS) {
            instruments[symbol] = state;
        }
    }

    /// Get state for a single instrument. Called by HTTP thread.
    InstrumentState get(SymbolId symbol) const {
        std::lock_guard<std::mutex> lock(mtx);
        if (symbol < MAX_INSTRUMENTS) {
            return instruments[symbol];
        }
        return {};
    }

    /// Atomic snapshot of all instruments. Called by HTTP thread.
    std::array<InstrumentState, MAX_INSTRUMENTS> snapshot() const {
        std::lock_guard<std::mutex> lock(mtx);
        return instruments;
    }

private:
    std::array<InstrumentState, MAX_INSTRUMENTS> instruments{};
    mutable std::mutex mtx;
};

} // namespace exchange
