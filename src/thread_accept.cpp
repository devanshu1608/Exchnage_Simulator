#include "app_threads.hpp"
#include <tcp_server.hpp>
#include <tcp_client.hpp>
#include <protocol.hpp>
#include <cstdio>
#include <cstring>

namespace exchange {
using namespace exchange::net;

void runAcceptThread(uint16_t port, SessionManager& sessions, net::EpollMgr& epoll, std::atomic<bool>& gRunning) {
    net::TcpServer server(port);

    server.start([&](net::TcpSocket client) {
        // when a new client is accepted this function is called
        LoginRequestMsg login{};
        ssize_t n = client.recvRaw(reinterpret_cast<char*>(&login), sizeof(LoginRequestMsg));
        if (n != static_cast<ssize_t>(sizeof(LoginRequestMsg))) {
            std::fprintf(stderr, "failed to read login request from fd=%d\n", client.fd());
            return; // socket closes on destruction
        }

        if (login.header.type != MsgType::LoginRequest) {
            std::fprintf(stderr, "invalid message type from fd=%d\n", client.fd());
            return;
        }

        std::printf("trader %u logged in on fd=%d\n", login.traderId, client.fd());

        // send login response
        LoginResponseMsg resp{};
        resp.header.length = sizeof(LoginResponseMsg);
        resp.header.type = MsgType::LoginResponse;
        resp.accepted = true;
        client.sendRaw(reinterpret_cast<const char*>(&resp), sizeof(resp));

        // register with session manager
        int fd = client.fd();
        TraderId traderId = login.traderId;
        sessions.add(fd, traderId);

        // set non blocking for epoll
        client.setNonBlocking();

        // register fd with epoll
        epoll.add(fd);

        // release ownership fd stays open and managed by epoll
        client.release();

        std::printf("trader %u registered with epoll\n", traderId);
    });
}

} // namespace exchange
