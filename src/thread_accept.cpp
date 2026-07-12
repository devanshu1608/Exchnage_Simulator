#include "app_threads.hpp"
#include <tcp_server.hpp>
#include <tcp_client.hpp>
#include <protocol.hpp>
#include <cstdio>
#include <cstring>

namespace exchange {
using namespace exchange::net;

void runAcceptThread(uint16_t port, SessionManager& sessions, net::EpollMgr& epoll, std::atomic<bool>& /*gRunning*/) {
    net::TcpServer server(port);

    server.start([&](net::TcpSocket client) {
        // 1. Read LoginRequestMsg (blocking — this thread only handles this one client)
        LoginRequestMsg login{};
        ssize_t n = client.recvRaw(reinterpret_cast<char*>(&login), sizeof(LoginRequestMsg));
        if (n != static_cast<ssize_t>(sizeof(LoginRequestMsg))) {
            std::fprintf(stderr, "[Login] Failed to read LoginRequestMsg from fd=%d\n", client.fd());
            return; // Socket closes on destruction
        }

        if (login.header.type != MsgType::LoginRequest) {
            std::fprintf(stderr, "[Login] Invalid message type from fd=%d\n", client.fd());
            return;
        }

        std::printf("[Login] Trader %u logged in on fd=%d\n", login.traderId, client.fd());

        // 2. Send LoginResponse
        LoginResponseMsg resp{};
        resp.header.length = sizeof(LoginResponseMsg);
        resp.header.type = MsgType::LoginResponse;
        resp.accepted = true;
        client.sendRaw(reinterpret_cast<const char*>(&resp), sizeof(resp));

        // 3. Register with session manager
        int fd = client.fd();
        TraderId traderId = login.traderId;
        sessions.add(fd, traderId);

        // 4. Set non-blocking for epoll
        client.setNonBlocking();

        // 5. Register fd with epoll (thread-safe: epoll_ctl is safe to call concurrently with epoll_wait)
        epoll.add(fd);

        // 6. Release ownership — fd stays open, managed by epoll now
        client.release();

        std::printf("[Login] Trader %u registered with epoll, login thread exiting\n", traderId);
    });
}

} // namespace exchange
