#pragma once
#include "types.hpp"
#include <type_traits>

namespace exchange {

struct alignas(CACHE_LINE_SIZE) OrderRequest {
    OrderId     m_orderId{0};
    OrderId     m_clientOrderId{0}; // Client internal order ID
    Price       m_price{0};
    Timestamp   m_timestamp{0};
    TraderId    m_traderId{0};
    SymbolId    m_symbolId{0};
    Quantity    m_quantity{0};
    Side        m_side{Side::Buy};
    RequestType m_requestType{RequestType::New};
};

struct BookElement {
    OrderId     m_id{0};
    TraderId    m_traderId{0};
    Quantity    m_quantity{0};
};

static_assert(std::is_trivially_copyable_v<OrderRequest>,
    "OrderRequest must be trivially copyable for lock-free queues");

static_assert(std::is_trivially_copyable_v<BookElement>,
    "BookElement must be trivially copyable");

/// Private confirmation to send back to a specific trader
struct OrderConfirm {
    enum class Type : uint8_t { NewAck, ModifyAck, CancelAck, Trade, Reject };
    Type         type{Type::NewAck};
    OrderId      orderId{0};       // Exchange Order ID
    OrderId      clientOrderId{0}; // Client Internal Order ID
    TraderId     traderId{0};
    SymbolId     symbolId{0};
    Price        price{0};
    Quantity     qty{0};           // Trade qty, or remaining qty, etc.
    RejectReason reason;      // only meaningful for Reject
};

enum class MdUpdateType : uint8_t { New = 1, Modify = 2, Cancel = 3, Trade = 4 };

/// Market data update for all participants
struct MdUpdate {
    SymbolId symbolId;
    OrderId  orderId; // The ID of the order being added/modified/traded
    Side     side;
    Price    price;
    Quantity qty;  // delta/order quantity
    MdUpdateType updateType;
};

} // namespace exchange
