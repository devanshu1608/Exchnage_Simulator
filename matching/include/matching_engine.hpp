#pragma once

#include <types.hpp>
#include <order.hpp>
#include <order_book.hpp>
#include <unordered_map>
#include <memory>
#include <vector>
#include <atomic>

namespace exchange {

/// @brief Routes orders to the correct OrderBook.
class MatchingEngine {
public:
    MatchingEngine() = default;

    /// Initialize books for the given symbols
    void initialize(const std::vector<SymbolId>& symbols);

    /// Route a new order to the correct book
    void onOrder(OrderRequest order, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut);

    /// Route a cancel request
    void onCancel(OrderId orderId, SymbolId symbolId, TraderId traderId, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut);

    /// Route a modify request
    void onModify(OrderId orderId, SymbolId symbolId, TraderId traderId, Price newPrice, Quantity newQty, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut);

    Price getBestBidPrice(SymbolId symbolId) const {
        auto it = books.find(symbolId);
        return it != books.end() ? it->second->getBestBidPrice() : 0;
    }
    Quantity getBestBidQty(SymbolId symbolId) const {
        auto it = books.find(symbolId);
        return it != books.end() ? it->second->getBestBidQty() : 0;
    }
    Price getBestAskPrice(SymbolId symbolId) const {
        auto it = books.find(symbolId);
        return it != books.end() ? it->second->getBestAskPrice() : 0;
    }
    Quantity getBestAskQty(SymbolId symbolId) const {
        auto it = books.find(symbolId);
        return it != books.end() ? it->second->getBestAskQty() : 0;
    }

private:
    std::unordered_map<SymbolId, std::unique_ptr<OrderBook>> books;
    std::atomic<OrderId> nextOrderId{1};
};

} // namespace exchange
