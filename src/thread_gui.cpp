#include "app_threads.hpp"
#include <cstdio>
#include <thread>
#include <chrono>
#include <protocol.hpp>

namespace exchange {

void runGuiThread(MarketState& marketState, std::atomic<bool>& gRunning) {
    constexpr const char* symbols[] = {"RELIANCE", "TCS", "INFY", "HDFCBANK", "ICICIBANK"};
    
    while (gRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::printf("\033[2J\033[H");
        std::printf("market state terminal gui\n\n");
        std::printf("-----------------------------------------------------------------------\n");
        
        auto snapshot = marketState.snapshot();
        for (size_t i = 0; i < snapshot.size(); ++i) {
            const auto& state = snapshot[i];
            if (state.lastTradePrice != 0 || state.bestBid != 0 || state.bestAsk != 0) {
                const char* sym = (i < 5) ? symbols[i] : "UNKNOWN";
                std::printf("  %s | ASK: %u x %u | BID: %u x %u | LAST: %u x %u | VOL: %u\n",
                    sym,
                    state.bestAsk, state.askVolume,
                    state.bestBid, state.bidVolume,
                    state.lastTradePrice, state.lastTradeQty,
                    state.totalTradedVolume);
            }
        }
    }
}

} // namespace exchange
