/// @file client/src/main.cpp
/// @brief Interactive test client for the exchange simulator.
///
/// Connects to the server, logs in, reads orders from terminal interactively,
/// and prints received confirmations and market updates.

#include <tcp_client.hpp>
#include <tcp_socket.hpp>
#include <protocol.hpp>
#include <types.hpp>

#include <iostream>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <sys/socket.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <algorithm>

using namespace exchange;
using namespace exchange::net;

static std::atomic<bool> gRunning{true};
static std::unordered_map<OrderId, OrderId> internalToExchange;
static std::mutex mapMutex;

static void signalHandler(int) {
    gRunning = false;
}

/// Helper: send a complete struct as raw bytes.
template<typename T>
bool sendMsg(TcpSocket& sock, const T& msg) {
    ssize_t n = sock.sendRaw(reinterpret_cast<const char*>(&msg), sizeof(T));
    return n == static_cast<ssize_t>(sizeof(T));
}

/// Helper: receive a complete struct as raw bytes (blocking).
template<typename T>
bool recvMsg(TcpSocket& sock, T& msg) {
    char* ptr = reinterpret_cast<char*>(&msg);
    std::size_t remaining = sizeof(T);
    while (remaining > 0) {
        ssize_t n = sock.recvRaw(ptr, remaining);
        if (n <= 0) return false;
        ptr += n;
        remaining -= static_cast<std::size_t>(n);
    }
    return true;
}

void printNewOrderConfirm(const NewOrderConfirmMsg& msg) {
    std::printf("\n  [NewOrderConfirm] internalId=%lu exchangeOrderId=%lu\n> ", msg.clientOrderId, msg.orderId);
    std::fflush(stdout);
}

void printModifyOrderConfirm(const ModifyOrderConfirmMsg& msg) {
    std::printf("\n  [ModifyOrderConfirm] exchangeOrderId=%lu\n> ", msg.orderId);
    std::fflush(stdout);
}

void printCancelOrderConfirm(const CancelOrderConfirmMsg& msg) {
    std::printf("\n  [CancelOrderConfirm] exchangeOrderId=%lu\n> ", msg.orderId);
    std::fflush(stdout);
}

void printTradeConfirm(const TradeConfirmMsg& msg) {
    std::printf("\n  [TradeConfirm] exchangeOrderId=%lu price=%u qty=%u\n> ",
        msg.orderId, msg.price, msg.qty);
    std::fflush(stdout);
}

void printReject(const RejectMsg& msg) {
    std::printf("\n  [Reject] exchangeOrderId=%lu reason=%u\n> ",
        msg.orderId, static_cast<unsigned>(msg.reason));
    std::fflush(stdout);
}

void printMarketUpdate(const MarketUpdateMsg& msg) {
    constexpr const char* symbols[] = {"RELIANCE", "TCS", "INFY", "HDFCBANK", "ICICIBANK"};
    const char* sym = (msg.symbolId < 5) ? symbols[msg.symbolId] : "UNKNOWN";
    
    const char* typeStr = "UNKNOWN";
    if (msg.updateType == MdUpdateType::New) typeStr = "NEW";
    else if (msg.updateType == MdUpdateType::Modify) typeStr = "MODIFY";
    else if (msg.updateType == MdUpdateType::Cancel) typeStr = "CANCEL";
    else if (msg.updateType == MdUpdateType::Trade) typeStr = "TRADE";

    std::printf("\n  [MarketUpdate] %s %s orderId=%lu price=%u qty=%u (Reason: %s)\n> ",
        sym,
        msg.side == Side::Buy ? "BID" : "ASK",
        msg.orderId,
        msg.price,
        msg.qty,
        typeStr);
    std::fflush(stdout);
}

void recvThreadFunc(TcpSocket& sock) {
    while (gRunning) {
        MsgHeader header;
        if (!recvMsg(sock, header)) {
            std::printf("\nServer disconnected.\n");
            gRunning = false;
            break;
        }

        // Read the remaining bytes of the message
        std::size_t bodySize = header.length - sizeof(MsgHeader);
        char body[256];
        if (bodySize > 0 && bodySize <= sizeof(body)) {
            char* ptr = body;
            std::size_t remaining = bodySize;
            while (remaining > 0) {
                ssize_t n = sock.recvRaw(ptr, remaining);
                if (n <= 0) { gRunning = false; break; }
                ptr += n;
                remaining -= static_cast<std::size_t>(n);
            }
        }

        switch (header.type) {
        case MsgType::NewOrderConfirm: {
            NewOrderConfirmMsg msg;
            msg.header = header;
            std::memcpy(reinterpret_cast<char*>(&msg) + sizeof(MsgHeader),
                        body, sizeof(NewOrderConfirmMsg) - sizeof(MsgHeader));
            printNewOrderConfirm(msg);
            
            std::lock_guard<std::mutex> lock(mapMutex);
            internalToExchange[msg.clientOrderId] = msg.orderId;
            break;
        }
        case MsgType::ModifyOrderConfirm: {
            ModifyOrderConfirmMsg msg;
            msg.header = header;
            std::memcpy(reinterpret_cast<char*>(&msg) + sizeof(MsgHeader),
                        body, sizeof(ModifyOrderConfirmMsg) - sizeof(MsgHeader));
            printModifyOrderConfirm(msg);
            break;
        }
        case MsgType::CancelOrderConfirm: {
            CancelOrderConfirmMsg msg;
            msg.header = header;
            std::memcpy(reinterpret_cast<char*>(&msg) + sizeof(MsgHeader),
                        body, sizeof(CancelOrderConfirmMsg) - sizeof(MsgHeader));
            printCancelOrderConfirm(msg);
            break;
        }
        case MsgType::TradeConfirm: {
            TradeConfirmMsg msg;
            msg.header = header;
            std::memcpy(reinterpret_cast<char*>(&msg) + sizeof(MsgHeader),
                        body, sizeof(TradeConfirmMsg) - sizeof(MsgHeader));
            printTradeConfirm(msg);
            break;
        }
        case MsgType::Reject: {
            RejectMsg msg;
            msg.header = header;
            std::memcpy(reinterpret_cast<char*>(&msg) + sizeof(MsgHeader),
                        body, sizeof(RejectMsg) - sizeof(MsgHeader));
            printReject(msg);
            break;
        }
        case MsgType::MarketUpdate: {
            MarketUpdateMsg msg;
            msg.header = header;
            std::memcpy(reinterpret_cast<char*>(&msg) + sizeof(MsgHeader),
                        body, sizeof(MarketUpdateMsg) - sizeof(MsgHeader));
            printMarketUpdate(msg);
            break;
        }
        default:
            std::printf("\n  [Unknown] type=%u length=%u\n> ",
                static_cast<unsigned>(header.type), header.length);
            std::fflush(stdout);
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::signal(SIGINT, signalHandler);

    std::printf("=== Interactive Exchange Client ===\n");
    std::printf("Enter server IP, port, traderId (e.g. 127.0.0.1, 9001, 1):\n> ");
    std::fflush(stdout);

    std::string initLine;
    if (!std::getline(std::cin, initLine)) {
        return 0;
    }

    // Clean commas
    for (char& c : initLine) {
        if (c == ',') c = ' ';
    }

    std::stringstream initSS(initLine);
    std::string hostStr;
    uint16_t port;
    TraderId traderId;

    if (!(initSS >> hostStr >> port >> traderId)) {
        std::fprintf(stderr, "Invalid input format.\n");
        return 1;
    }

    std::printf("Connecting to %s:%u as trader %u...\n", hostStr.c_str(), port, traderId);

    // ── Connect ──
    TcpSocket sock;
    try {
        sock = TcpClient::connect(hostStr, port);
        std::printf("Connected (fd=%d)\n", sock.fd());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Connection failed: %s\n", e.what());
        return 1;
    }

    // ── Login ──
    LoginRequestMsg login{};
    login.header.length = sizeof(LoginRequestMsg);
    login.header.type = MsgType::LoginRequest;
    login.traderId = traderId;

    if (!sendMsg(sock, login)) {
        std::fprintf(stderr, "Failed to send LoginRequestMsg\n");
        return 1;
    }

    LoginResponseMsg resp{};
    if (!recvMsg(sock, resp)) {
        std::fprintf(stderr, "Failed to receive LoginResponseMsg\n");
        return 1;
    }

    std::printf("Login: %s\n\n", resp.accepted ? "ACCEPTED" : "REJECTED");
    if (!resp.accepted) return 1;

    // Start receive thread
    std::thread recvThread(recvThreadFunc, std::ref(sock));

    constexpr const char* symbols[] = {"RELIANCE", "TCS", "INFY", "HDFCBANK", "ICICIBANK"};

    std::printf("Ready. Enter requests (N/M/C format) or 'quit' to exit.\n");
    std::printf("Formats:\n");
    std::printf("  N, internal_orderid, symbol_id, BUY|SELL, price, qty\n");
    std::printf("  M, internal_orderid, symbol_id, price, qty\n");
    std::printf("  C, internal_orderid, symbol_id\n");

    std::string line;
    while (gRunning) {
        std::printf("> ");
        std::fflush(stdout);
        
        if (!std::getline(std::cin, line)) break;
        if (line.empty() || line[0] == '#') continue;
        
        if (line == "quit" || line == "q" || line == "exit") {
            gRunning = false;
            break;
        }
        
        // Remove commas
        for (char& c : line) {
            if (c == ',') c = ' ';
        }
        
        std::stringstream ss(line);
        std::string action;
        ss >> action;

        if (action == "N") {
            OrderId internalId;
            SymbolId symId;
            std::string sideStr;
            uint32_t price;
            Quantity qty;
            if (!(ss >> internalId >> symId >> sideStr >> price >> qty)) {
                std::printf("Invalid format for N\n");
                continue;
            }

            NewOrderRequestMsg order{};
            order.header.length = sizeof(NewOrderRequestMsg);
            order.header.type = MsgType::NewOrderRequest;
            order.clientOrderId = internalId;
            order.symbolId = symId;
            order.side = (sideStr == "BUY") ? Side::Buy : Side::Sell;
            order.price = price;
            order.qty = qty;

            const char* sym = (symId < 5) ? symbols[symId] : "?";
            std::printf("Sending: NEW internalId=%lu %s %s %u x %u\n",
                internalId, sym, sideStr.c_str(), price, qty);

            if (!sendMsg(sock, order)) { gRunning = false; break; }

        } else if (action == "M") {
            OrderId internalId;
            SymbolId symId;
            uint32_t price;
            Quantity qty;
            if (!(ss >> internalId >> symId >> price >> qty)) {
                std::printf("Invalid format for M\n");
                continue;
            }

            OrderId exchId = 0;
            {
                std::lock_guard<std::mutex> lock(mapMutex);
                auto it = internalToExchange.find(internalId);
                if (it != internalToExchange.end()) {
                    exchId = it->second;
                }
            }
            
            if (exchId == 0) {
                std::printf("Error: Unknown internalOrderId %lu (Not accepted by exchange yet?)\n", internalId);
                continue;
            }

            ModifyOrderRequestMsg order{};
            order.header.length = sizeof(ModifyOrderRequestMsg);
            order.header.type = MsgType::ModifyOrderRequest;
            order.orderId = exchId;
            order.symbolId = symId;
            order.price = price;
            order.qty = qty;

            const char* sym = (symId < 5) ? symbols[symId] : "?";
            std::printf("Sending: MOD internalId=%lu exchId=%lu %s %u x %u\n",
                internalId, exchId, sym, price, qty);

            if (!sendMsg(sock, order)) { gRunning = false; break; }

        } else if (action == "C") {
            OrderId internalId;
            SymbolId symId;
            if (!(ss >> internalId >> symId)) {
                std::printf("Invalid format for C\n");
                continue;
            }

            OrderId exchId = 0;
            {
                std::lock_guard<std::mutex> lock(mapMutex);
                auto it = internalToExchange.find(internalId);
                if (it != internalToExchange.end()) {
                    exchId = it->second;
                }
            }
            
            if (exchId == 0) {
                std::printf("Error: Unknown internalOrderId %lu (Not accepted by exchange yet?)\n", internalId);
                continue;
            }

            CancelOrderRequestMsg order{};
            order.header.length = sizeof(CancelOrderRequestMsg);
            order.header.type = MsgType::CancelOrderRequest;
            order.orderId = exchId;
            order.symbolId = symId;

            const char* sym = (symId < 5) ? symbols[symId] : "?";
            std::printf("Sending: CXL internalId=%lu exchId=%lu %s\n",
                internalId, exchId, sym);

            if (!sendMsg(sock, order)) { gRunning = false; break; }
        } else {
            std::printf("Unknown action: %s\n", action.c_str());
        }
    }

    // shutdown socket so recv thread unblocks
    ::shutdown(sock.fd(), SHUT_RDWR);

    if (recvThread.joinable()) {
        recvThread.join();
    }

    std::printf("\nClient exiting.\n");
    return 0;
}
