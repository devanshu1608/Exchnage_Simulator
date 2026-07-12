#include <order_book.hpp>
#include <algorithm>

namespace exchange {

// ─── Template helpers ───────────────────────────────────────────────

template<Side S>
PriceLevel& OrderBook::getOrCreateLevel(Price price) {
    auto& levels = getBook<S>();
    auto it = levels.find(price);
    if (it == levels.end()) {
        it = levels.emplace(price, PriceLevel(price)).first;
    }
    return it->second;
}

template<Side S>
void OrderBook::removeFromLevel(BookElement* elem, Price price) {
    auto& levels = getBook<S>();
    auto it = levels.find(price);
    if (it == levels.end()) return;

    it->second.removeOrder(elem);
    if (it->second.isEmpty()) levels.erase(it);
}

template<Side S>
void OrderBook::insertIntoLevel(BookElement* elem, Price price) {
    PriceLevel& level = getOrCreateLevel<S>(price);
    level.addOrder(elem);
}

template<Side PassiveSide>
void OrderBook::matchAgainst(const OrderRequest& incoming, Quantity& remainingQty,
                                          std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut) {
    auto& levels = getBook<PassiveSide>();
    
    while (remainingQty > 0 && !levels.empty()) {
        auto bestIt = levels.begin();
        Price levelPrice = bestIt->first;
        
        bool canTrade = false;
        if constexpr (PassiveSide == Side::Sell) {
            canTrade = (levelPrice <= incoming.m_price);
        } else {
            // PassiveSide == Buy. Bids are sorted descending due to std::greater, so begin() is the highest price.
            canTrade = (levelPrice >= incoming.m_price);
        }
        
        if (!canTrade) break;

        PriceLevel& level = bestIt->second;

        while (remainingQty > 0 && !level.isEmpty()) {
            BookElement* passiveOrder = level.peekFirst();
            
            // Self-Trade Prevention (Cancel Resting/Passive)
            if (passiveOrder->m_traderId == incoming.m_traderId) {
                Quantity cancelledQty = passiveOrder->m_quantity;
                confirmsOut.push_back(OrderConfirm{
                    OrderConfirm::Type::CancelAck, passiveOrder->m_id, 0, passiveOrder->m_traderId, symbol,
                    0, cancelledQty, RejectReason::SelfTrade
                });
                
                mdOut.push_back(MdUpdate{symbol, passiveOrder->m_id, PassiveSide, levelPrice, cancelledQty, MdUpdateType::Cancel});

                level.removeOrder(passiveOrder);
                orderMap.erase(passiveOrder->m_id);
                orderDetails.erase(passiveOrder->m_id);
                delete passiveOrder;
                continue;
            }

            Quantity tradeQty = std::min(remainingQty, passiveOrder->m_quantity);

            passiveOrder->m_quantity -= tradeQty;
            remainingQty -= tradeQty;

            // TradeConfirm for resting order
            confirmsOut.push_back(OrderConfirm{
                OrderConfirm::Type::Trade, passiveOrder->m_id, 0, passiveOrder->m_traderId, symbol,
                levelPrice, tradeQty, RejectReason::UnknownOrder
            });

            // TradeConfirm for aggressor
            confirmsOut.push_back(OrderConfirm{
                OrderConfirm::Type::Trade, incoming.m_orderId, incoming.m_clientOrderId, incoming.m_traderId, symbol,
                levelPrice, tradeQty, RejectReason::UnknownOrder
            });

            // Emit trade MD updates
            mdOut.push_back(MdUpdate{symbol, passiveOrder->m_id, PassiveSide, levelPrice, tradeQty, MdUpdateType::Trade});
            mdOut.push_back(MdUpdate{symbol, incoming.m_orderId, incoming.m_side, incoming.m_price, tradeQty, MdUpdateType::Trade});

            if (passiveOrder->m_quantity == 0) {
                level.removeOrder(passiveOrder);
                orderMap.erase(passiveOrder->m_id);
                orderDetails.erase(passiveOrder->m_id);
                delete passiveOrder;
            } else {
                level.reduceQty(tradeQty);
            }
        }

        if (level.isEmpty()) {
            levels.erase(bestIt);
        }
    }
}

// ─── Public API ─────────────────────────────────────────────────────

void OrderBook::addOrder(const OrderRequest& incoming, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut) {
    // 0. Confirm the new order was accepted FIRST, before any trades occur
    confirmsOut.push_back(OrderConfirm{
        OrderConfirm::Type::NewAck, incoming.m_orderId, incoming.m_clientOrderId, incoming.m_traderId, symbol,
        0, 0, RejectReason::UnknownOrder
    });

    mdOut.push_back(MdUpdate{symbol, incoming.m_orderId, incoming.m_side, incoming.m_price, incoming.m_quantity, MdUpdateType::New});

    // 1. Cross the spread
    Quantity remainingQty = incoming.m_quantity;

    if (incoming.m_side == Side::Buy) {
        matchAgainst<Side::Sell>(incoming, remainingQty, confirmsOut, mdOut);
    } else {
        matchAgainst<Side::Buy>(incoming, remainingQty, confirmsOut, mdOut);
    }

    if (remainingQty == 0) return;

    // 3. Add to book
    BookElement* elem = new BookElement{incoming.m_orderId, incoming.m_traderId, remainingQty};
    orderMap[incoming.m_orderId] = elem;
    orderDetails[incoming.m_orderId] = {incoming.m_side, incoming.m_price, remainingQty};

    if (incoming.m_side == Side::Buy) {
        insertIntoLevel<Side::Buy>(elem, incoming.m_price);
    } else {
        insertIntoLevel<Side::Sell>(elem, incoming.m_price);
    }
}

void OrderBook::cancelOrder(OrderId id, TraderId traderId, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut) {
    auto it = orderMap.find(id);
    if (it == orderMap.end()) {
        confirmsOut.push_back(OrderConfirm{
            OrderConfirm::Type::Reject, id, 0, traderId, symbol,
            0, 0, RejectReason::UnknownOrder
        });
        return;
    }

    BookElement* elem = it->second;
    if (elem->m_traderId != traderId) {
        confirmsOut.push_back(OrderConfirm{
            OrderConfirm::Type::Reject, id, 0, traderId, symbol,
            0, 0, RejectReason::UnknownOrder
        });
        return;
    }

    // Look up details
    auto detailIt = orderDetails.find(id);
    Side side = detailIt->second.side;
    Price price = detailIt->second.price;
    Quantity qty = elem->m_quantity;

    if (side == Side::Buy) {
        removeFromLevel<Side::Buy>(elem, price);
    } else {
        removeFromLevel<Side::Sell>(elem, price);
    }

    orderMap.erase(it);
    orderDetails.erase(detailIt);
    delete elem;

    mdOut.push_back(MdUpdate{symbol, id, side, price, qty, MdUpdateType::Cancel});

    confirmsOut.push_back(OrderConfirm{
        OrderConfirm::Type::CancelAck, id, 0, traderId, symbol,
        0, 0, RejectReason::UnknownOrder
    });
}

void OrderBook::modifyOrder(OrderId id, TraderId traderId, Price newPrice, Quantity newQty, std::vector<OrderConfirm>& confirmsOut, std::vector<MdUpdate>& mdOut) {
    auto it = orderMap.find(id);
    if (it == orderMap.end()) {
        confirmsOut.push_back(OrderConfirm{
            OrderConfirm::Type::Reject, id, 0, traderId, symbol,
            0, 0, RejectReason::UnknownOrder
        });
        return;
    }

    BookElement* elem = it->second;
    if (elem->m_traderId != traderId) {
        confirmsOut.push_back(OrderConfirm{
            OrderConfirm::Type::Reject, id, 0, traderId, symbol,
            0, 0, RejectReason::UnknownOrder
        });
        return;
    }

    auto detailIt = orderDetails.find(id);
    Side side = detailIt->second.side;
    Price oldPrice = detailIt->second.price;
    Quantity oldQty = detailIt->second.quantity;

    if (oldPrice == newPrice && newQty < oldQty) {
        // Priority remains same
        elem->m_quantity = newQty;
        detailIt->second.quantity = newQty;
    } 
    else {
        if (side == Side::Buy) {
            removeFromLevel<Side::Buy>(elem, oldPrice);
        } else {
            removeFromLevel<Side::Sell>(elem, oldPrice);
        }

        // Update element
        elem->m_quantity = newQty;
        detailIt->second.price = newPrice;
        detailIt->second.quantity = newQty;

        // Insert at new price level
        if (side == Side::Buy) {
            insertIntoLevel<Side::Buy>(elem, newPrice);
        } else {
            insertIntoLevel<Side::Sell>(elem, newPrice);
        }
    }

    mdOut.push_back(MdUpdate{symbol, id, side, newPrice, newQty, MdUpdateType::Modify});

    confirmsOut.push_back(OrderConfirm{
        OrderConfirm::Type::ModifyAck, id, 0, traderId, symbol,
        0, 0, RejectReason::UnknownOrder
    });
}
} // namespace exchange
