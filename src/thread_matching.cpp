#include "app_threads.hpp"
#include <matching_engine.hpp>
#include <protocol.hpp>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>

namespace exchange {
using namespace exchange::net;

void runMatchingThread(SPSCQueue<RawMessage, 65536>& rawQueue, SessionManager& sessions, MarketState& marketState, std::atomic<bool>& gRunning) {
    MatchingEngine engine;
    engine.initialize({0, 1, 2, 3, 4});

    std::vector<OrderConfirm> confirms;
    std::vector<MdUpdate> mdUpdates;

    std::printf("[Matching] Engine started\n");

    while (gRunning) {
        RawMessage raw;
        if (!rawQueue.try_pop(raw)) {
            continue;
        }

        // Parse message header
        if (raw.length < sizeof(MsgHeader)) continue;
        MsgHeader header;
        std::memcpy(&header, raw.data, sizeof(MsgHeader));

        // Parse wire message into internal OrderRequest
        OrderRequest req{};
        switch (header.type) {
        case MsgType::NewOrderRequest: {
            if (raw.length < sizeof(NewOrderRequestMsg)) continue;
            NewOrderRequestMsg msg;
            std::memcpy(&msg, raw.data, sizeof(NewOrderRequestMsg));
            req = toOrderRequest(msg, raw.traderId);
            break;
        }
        case MsgType::CancelOrderRequest: {
            if (raw.length < sizeof(CancelOrderRequestMsg)) continue;
            CancelOrderRequestMsg msg;
            std::memcpy(&msg, raw.data, sizeof(CancelOrderRequestMsg));
            req = toOrderRequest(msg, raw.traderId);
            break;
        }
        case MsgType::ModifyOrderRequest: {
            if (raw.length < sizeof(ModifyOrderRequestMsg)) continue;
            ModifyOrderRequestMsg msg;
            std::memcpy(&msg, raw.data, sizeof(ModifyOrderRequestMsg));
            req = toOrderRequest(msg, raw.traderId);
            break;
        }
        default:
            std::fprintf(stderr, "[Matching] Unknown message type %u from trader=%u\n",
                static_cast<unsigned>(header.type), raw.traderId);
            continue;
        }

        confirms.clear();
        mdUpdates.clear();

        switch (req.m_requestType) {
        case RequestType::New:
            std::printf("[Matching] NewOrder id=%lu symbol=%u side=%s price=%u qty=%u trader=%u\n",
                req.m_orderId, req.m_symbolId,
                req.m_side == Side::Buy ? "BUY" : "SELL",
                req.m_price, req.m_quantity, req.m_traderId);
            engine.onOrder(req, confirms, mdUpdates);
            break;

        case RequestType::Cancel:
            std::printf("[Matching] CancelOrder id=%lu symbol=%u trader=%u\n",
                req.m_orderId, req.m_symbolId, req.m_traderId);
            engine.onCancel(req.m_orderId, req.m_symbolId, req.m_traderId, confirms, mdUpdates);
            break;

        case RequestType::Modify:
            std::printf("[Matching] ModifyOrder id=%lu symbol=%u newPrice=%u newQty=%u trader=%u\n",
                req.m_orderId, req.m_symbolId,
                req.m_price, req.m_quantity, req.m_traderId);
            engine.onModify(req.m_orderId, req.m_symbolId, req.m_traderId, req.m_price, req.m_quantity, confirms, mdUpdates);
            break;
        }

        // Send private confirmations to the relevant traders
        for (const auto& confirm : confirms) {
            int fd = sessions.getFd(confirm.traderId);
            if (fd < 0) continue;

            switch (confirm.type) {
            case OrderConfirm::Type::NewAck: {
                auto msg = toNewOrderConfirmMsg(confirm);
                ::send(fd, &msg, sizeof(msg), MSG_NOSIGNAL);
                break;
            }
            case OrderConfirm::Type::ModifyAck: {
                auto msg = toModifyOrderConfirmMsg(confirm);
                ::send(fd, &msg, sizeof(msg), MSG_NOSIGNAL);
                break;
            }
            case OrderConfirm::Type::CancelAck: {
                auto msg = toCancelOrderConfirmMsg(confirm);
                ::send(fd, &msg, sizeof(msg), MSG_NOSIGNAL);
                break;
            }
            case OrderConfirm::Type::Trade: {
                auto msg = toTradeConfirmMsg(confirm);
                ::send(fd, &msg, sizeof(msg), MSG_NOSIGNAL);
                break;
            }
            case OrderConfirm::Type::Reject: {
                auto msg = toRejectMsg(confirm);
                ::send(fd, &msg, sizeof(msg), MSG_NOSIGNAL);
                break;
            }
            }
        }

        // Multicast MarketUpdates to ALL connected clients
        for (const auto& md : mdUpdates) {
            auto msg = toMarketUpdateMsg(md);
            auto allFds = sessions.getAllFds();
            for (int fd : allFds) {
                ::send(fd, &msg, sizeof(msg), MSG_NOSIGNAL);
            }
        }

        // Update market state for GUI
        if (req.m_requestType == RequestType::New || req.m_requestType == RequestType::Modify || req.m_requestType == RequestType::Cancel) {
            auto state = marketState.get(req.m_symbolId);
            
            // First apply trade updates
            for (const auto& confirm : confirms) {
                if (confirm.type == OrderConfirm::Type::Trade) {
                    state.lastTradePrice = confirm.price;
                    state.lastTradeQty = confirm.qty;
                    state.totalTradedVolume += confirm.qty;
                }
            }
            
            // Then apply best bid/ask
            state.bestBid = engine.getBestBidPrice(req.m_symbolId);
            state.bidVolume = engine.getBestBidQty(req.m_symbolId);
            state.bestAsk = engine.getBestAskPrice(req.m_symbolId);
            state.askVolume = engine.getBestAskQty(req.m_symbolId);
            
            marketState.update(req.m_symbolId, state);
        }
    }
}

} // namespace exchange
