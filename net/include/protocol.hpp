#pragma once

#include <types.hpp>
#include <order.hpp>
#include <cstdint>
#include <chrono>

namespace exchange::net {

#pragma pack(push, 1)

enum class MsgType : uint8_t {
    // Client → Exchange (Requests)
    NewOrderRequest    = 1,
    ModifyOrderRequest = 2,
    CancelOrderRequest = 3,
    LoginRequest       = 4,

    // Exchange → Client (Private confirmations)
    NewOrderConfirm    = 10,
    ModifyOrderConfirm = 11,
    CancelOrderConfirm = 12,
    TradeConfirm       = 13,
    Reject             = 14,
    LoginResponse      = 15,

    // Exchange → All Clients (Market Data)
    MarketUpdate       = 20
};

struct MsgHeader {
    uint16_t length;
    MsgType  type;
};

// ─── Client → Exchange ──────────────────────────────────────────────

struct NewOrderRequestMsg {
    MsgHeader header;
    OrderId   clientOrderId;
    SymbolId  symbolId;
    Side      side;
    Price     price;
    Quantity  qty;
};

struct ModifyOrderRequestMsg {
    MsgHeader header;
    OrderId   orderId;
    SymbolId  symbolId;
    Price     price;
    Quantity  qty;
};

struct CancelOrderRequestMsg {
    MsgHeader header;
    OrderId   orderId;
    SymbolId  symbolId;
};

struct LoginRequestMsg {
    MsgHeader header;
    TraderId  traderId;
};

// ─── Exchange → Client (Private) ────────────────────────────────────

struct NewOrderConfirmMsg {
    MsgHeader header;
    OrderId   clientOrderId;
    OrderId   orderId;
};

struct ModifyOrderConfirmMsg {
    MsgHeader header;
    OrderId   orderId;
};

struct CancelOrderConfirmMsg {
    MsgHeader header;
    OrderId   orderId;
};

struct TradeConfirmMsg {
    MsgHeader header;
    OrderId   orderId;
    Price     price;
    Quantity  qty;
};

struct RejectMsg {
    MsgHeader    header;
    OrderId      orderId;
    RejectReason reason;
};

struct LoginResponseMsg {
    MsgHeader header;
    bool      accepted;
};

// ─── Exchange → All Clients (Market Data) ───────────────────────────

struct MarketUpdateMsg {
    MsgHeader header;
    SymbolId  symbolId;
    OrderId   orderId;
    Side      side;
    Price     price;
    Quantity  qty;  // delta/order quantity
    MdUpdateType updateType;
};

#pragma pack(pop)

// ─── Wire → Internal Converters ─────────────────────────────────────

/// Convert a NewOrderRequestMsg (wire format) into an internal OrderRequest.
[[nodiscard]] inline OrderRequest toOrderRequest(
        const NewOrderRequestMsg& msg, TraderId traderId) noexcept {
    OrderRequest req{};
    req.m_clientOrderId = msg.clientOrderId;
    req.m_price       = msg.price;
    req.m_timestamp   = static_cast<Timestamp>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    req.m_traderId    = traderId;
    req.m_symbolId    = msg.symbolId;
    req.m_quantity    = msg.qty;
    req.m_side        = msg.side;
    req.m_requestType = RequestType::New;
    return req;
}

/// Convert a CancelOrderRequestMsg (wire format) into an internal OrderRequest.
[[nodiscard]] inline OrderRequest toOrderRequest(
        const CancelOrderRequestMsg& msg, TraderId traderId) noexcept {
    OrderRequest req{};
    req.m_orderId     = msg.orderId;
    req.m_timestamp   = static_cast<Timestamp>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    req.m_traderId    = traderId;
    req.m_symbolId    = msg.symbolId;
    req.m_requestType = RequestType::Cancel;
    return req;
}

/// Convert a ModifyOrderRequestMsg (wire format) into an internal OrderRequest.
[[nodiscard]] inline OrderRequest toOrderRequest(
        const ModifyOrderRequestMsg& msg, TraderId traderId) noexcept {
    OrderRequest req{};
    req.m_orderId     = msg.orderId;
    req.m_timestamp   = static_cast<Timestamp>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    req.m_traderId    = traderId;
    req.m_symbolId    = msg.symbolId;
    req.m_price       = msg.price;
    req.m_quantity    = msg.qty;
    req.m_requestType = RequestType::Modify;
    return req;
}

// ─── Internal → Wire Converters ─────────────────────────────────────

inline NewOrderConfirmMsg toNewOrderConfirmMsg(const OrderConfirm& confirm) noexcept {
    NewOrderConfirmMsg msg{};
    msg.header.length = sizeof(NewOrderConfirmMsg);
    msg.header.type = MsgType::NewOrderConfirm;
    msg.clientOrderId = confirm.clientOrderId;
    msg.orderId = confirm.orderId;
    return msg;
}

inline ModifyOrderConfirmMsg toModifyOrderConfirmMsg(const OrderConfirm& confirm) noexcept {
    ModifyOrderConfirmMsg msg{};
    msg.header.length = sizeof(ModifyOrderConfirmMsg);
    msg.header.type = MsgType::ModifyOrderConfirm;
    msg.orderId = confirm.orderId;
    return msg;
}

inline CancelOrderConfirmMsg toCancelOrderConfirmMsg(const OrderConfirm& confirm) noexcept {
    CancelOrderConfirmMsg msg{};
    msg.header.length = sizeof(CancelOrderConfirmMsg);
    msg.header.type = MsgType::CancelOrderConfirm;
    msg.orderId = confirm.orderId;
    return msg;
}

inline TradeConfirmMsg toTradeConfirmMsg(const OrderConfirm& confirm) noexcept {
    TradeConfirmMsg msg{};
    msg.header.length = sizeof(TradeConfirmMsg);
    msg.header.type = MsgType::TradeConfirm;
    msg.orderId = confirm.orderId;
    msg.price = confirm.price;
    msg.qty = confirm.qty;
    return msg;
}

inline RejectMsg toRejectMsg(const OrderConfirm& confirm) noexcept {
    RejectMsg msg{};
    msg.header.length = sizeof(RejectMsg);
    msg.header.type = MsgType::Reject;
    msg.orderId = confirm.orderId;
    msg.reason = confirm.reason;
    return msg;
}

inline MarketUpdateMsg toMarketUpdateMsg(const MdUpdate& md) noexcept {
    MarketUpdateMsg msg{};
    msg.header.length = sizeof(MarketUpdateMsg);
    msg.header.type = MsgType::MarketUpdate;
    msg.symbolId = md.symbolId;
    msg.orderId = md.orderId;
    msg.side = md.side;
    msg.price = md.price;
    msg.qty = md.qty;
    msg.updateType = md.updateType;
    return msg;
}

} // namespace exchange::net