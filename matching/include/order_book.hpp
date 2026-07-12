#pragma once

#include <types.hpp>
#include <order.hpp>
#include <price_level.hpp>
#include <map>
#include <unordered_map>
#include <vector>

namespace exchange {

/// @brief Order Book for a single instrument.
class OrderBook {
public:
    OrderBook(SymbolId sym) : symbol(sym) {}

    /// Process a new incoming order. Returns confirmations and market data updates.
    void addOrder(const OrderRequest& incoming, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut);

    /// Cancel an existing order
    void cancelOrder(OrderId id, TraderId traderId, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut);

    /// Modify an existing order (cancel + re-insert at new price/qty)
    void modifyOrder(OrderId id, TraderId traderId, Price newPrice, Quantity newQty, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut);

    Price getBestBidPrice() const { return bids.empty() ? 0 : bids.begin()->first; }
    Quantity getBestBidQty() const { return bids.empty() ? 0 : bids.begin()->second.getTotalQty(); }
    
    Price getBestAskPrice() const { return asks.empty() ? 0 : asks.begin()->first; }
    Quantity getBestAskQty() const { return asks.empty() ? 0 : asks.begin()->second.getTotalQty(); }

private:
    SymbolId symbol;

    std::map<Price, PriceLevel, std::greater<Price>> bids;
    std::map<Price, PriceLevel, std::less<Price>> asks;

    // Fast lookup for cancels
    std::unordered_map<OrderId, BookElement*> orderMap;

    // Store side + price + qty for each order (needed for cancel/modify lookup)
    struct OrderDetails {
        Side side;
        Price price;
        Quantity quantity;
    };
    std::unordered_map<OrderId, OrderDetails> orderDetails;

    // ─── Template helpers ───

    /// Helper to get the correct map based on side
    template<Side S>
    auto& getBook() {
        if constexpr (S == Side::Buy) return bids;
        else return asks;
    }

    /// Get or create a price level in a side's level map
    template<Side S>
    PriceLevel& getOrCreateLevel(Price price);

    /// Remove an order from its price level, erase empty levels
    template<Side S>
    void removeFromLevel(BookElement* elem, Price price);

    /// Insert an order into a price level
    template<Side S>
    void insertIntoLevel(BookElement* elem, Price price);

    /// Match an incoming order against the opposite side's book
    template<Side PassiveSide>
    void matchAgainst(const OrderRequest& incoming, Quantity& remainingQty,
                      std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut);

};

} // namespace exchange
