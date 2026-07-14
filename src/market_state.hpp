#pragma once
#include <types.hpp>
#include <array>
#include <mutex>

namespace exchange {

//used by terminal gui thread to print the current state of market
struct InstrumentState {
    Price     bestBid{0};
    Price     bestAsk{0};
    Quantity  bidVolume{0};
    Quantity  askVolume{0};
    Price     lastTradePrice{0};
    Quantity  lastTradeQty{0};
    Quantity  totalTradedVolume{0};
};

class MarketState {
public:

    void update(SymbolId symbol, const InstrumentState& state) {
        std::lock_guard<std::mutex> lock(mtx);
        if (symbol < MAX_INSTRUMENTS) {
            instruments[symbol] = state;
        }
    }

    InstrumentState get(SymbolId symbol) const {
        std::lock_guard<std::mutex> lock(mtx);
        if (symbol < MAX_INSTRUMENTS) {
            return instruments[symbol];
        }
        return {};
    }

    std::array<InstrumentState, MAX_INSTRUMENTS> snapshot() const {
        std::lock_guard<std::mutex> lock(mtx);
        return instruments;
    }

private:
    std::array<InstrumentState, MAX_INSTRUMENTS> instruments{};
    mutable std::mutex mtx;
};

} // namespace exchange
