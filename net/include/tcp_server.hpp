#pragma once
#include <tcp_socket.hpp>
#include <atomic>
#include <cstdint>
#include <functional>

namespace exchange::net {

class TcpServer {
public:
    using ClientHandler = std::function<void(TcpSocket client)>;

    explicit TcpServer(uint16_t port);
    ~TcpServer();

    // Non-copyable, non-movable
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    void start(ClientHandler handler);
    void stop();

private:
    TcpSocket listenSocket;
    uint16_t port;
    std::atomic<bool> running{false};

    void setup();
};

} // namespace exchange::net
