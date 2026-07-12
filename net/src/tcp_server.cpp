#include <tcp_server.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <cstdio>

namespace exchange::net {

TcpServer::TcpServer(uint16_t port) : port(port) {}

TcpServer::~TcpServer() {
    stop();
}

void TcpServer::setup() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("TcpServer: failed to create socket");
    }

    listenSocket = TcpSocket(fd);
    listenSocket.setReuseAddr();

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(listenSocket.fd(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("TcpServer: failed to bind");
    }

    if (::listen(listenSocket.fd(), SOMAXCONN) < 0) {
        throw std::runtime_error("TcpServer: failed to listen");
    }

    std::printf("[TcpServer] Listening on port %u\n", port);
}

void TcpServer::start(ClientHandler handler) {
    setup();
    running = true;

    while (running) {
        struct sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientFd = ::accept(listenSocket.fd(),
            reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);

        if (clientFd < 0) {
            if (!running) break;  // stop() was called
            continue;
        }

        std::printf("[TcpServer] Accepted client fd=%d\n", clientFd);

        TcpSocket client(clientFd);

        // Spawn a detached thread for the client handler
        std::thread([handler, sock = std::move(client)]() mutable {
            handler(std::move(sock));
        }).detach();
    }
}

void TcpServer::stop() {
    running = false;
    listenSocket.close();  // Unblocks accept()
}

} // namespace exchange::net
