#include <matching_engine.hpp>

namespace exchange {

void MatchingEngine::initialize(const std::vector<SymbolId>& symbols) {
    for (auto sym : symbols) {
        books[sym] = std::make_unique<OrderBook>(sym);
    }
}

void MatchingEngine::onOrder(OrderRequest order, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut) {
    order.m_orderId = nextOrderId++;
    auto it = books.find(order.m_symbolId);
    if (it != books.end()) {
        it->second->addOrder(order, confirmsOut, mdOut);
    }
}

void MatchingEngine::onCancel(OrderId orderId, SymbolId symbolId, TraderId traderId, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut) {
    auto it = books.find(symbolId);
    if (it != books.end()) {
        it->second->cancelOrder(orderId, traderId, confirmsOut, mdOut);
    }
}

void MatchingEngine::onModify(OrderId orderId, SymbolId symbolId, TraderId traderId, Price newPrice, Quantity newQty, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut) {
    auto it = books.find(symbolId);
    if (it != books.end()) {
        it->second->modifyOrder(orderId, traderId, newPrice, newQty, confirmsOut, mdOut);
    }
}

} // namespace exchange
