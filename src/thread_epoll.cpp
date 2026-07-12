#include "app_threads.hpp"
#include <protocol.hpp>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>

namespace exchange {
using namespace exchange::net;

void runEpollThread(net::EpollMgr& epoll, SessionManager& sessions, SPSCQueue<RawMessage, 65536>& rawQueue, std::atomic<bool>& /*gRunning*/) {
    std::printf("[Main] Starting epoll event loop\n");

    epoll.run(
        // onData callback
        [&](int fd) {
            ClientSession* session = sessions.getSession(fd);
            if (!session) return;

            // Read all available data (edge-triggered: must drain)
            char buf[4096];
            while (true) {
                ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
                if (n <= 0) {
                    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                        // Client disconnected or error
                        std::printf("[Main] Client fd=%d disconnected\n", fd);
                        epoll.remove(fd);
                        sessions.remove(fd);
                        ::close(fd);
                    }
                    break;
                }

                // Append to session recv buffer
                session->recvBuffer.insert(
                    session->recvBuffer.end(), buf, buf + n);
            }

            // Frame complete messages and push raw bytes to the queue.
            // Only reads MsgHeader.length for framing — no type inspection.
            while (session->recvBuffer.size() >= sizeof(MsgHeader)) {
                MsgHeader header;
                std::memcpy(&header, session->recvBuffer.data(), sizeof(MsgHeader));

                // Wait for complete message
                if (session->recvBuffer.size() < header.length) break;

                // Sanity check
                if (header.length > MAX_RAW_MSG_SIZE) {
                    std::fprintf(stderr, "[Main] Oversized message (%u bytes) from fd=%d, dropping\n",
                        header.length, fd);
                    session->recvBuffer.erase(
                        session->recvBuffer.begin(),
                        session->recvBuffer.begin() + header.length);
                    continue;
                }

                // Push raw bytes — no parsing, no type checking
                RawMessage raw;
                raw.traderId = session->traderId;
                raw.length   = header.length;
                std::memcpy(raw.data, session->recvBuffer.data(), header.length);
                rawQueue.push(raw);

                // Remove processed bytes from the recv buffer
                session->recvBuffer.erase(
                    session->recvBuffer.begin(),
                    session->recvBuffer.begin() + header.length);
            }
        },
        // onDisconnect callback
        [&](int fd) {
            std::printf("[Main] Client fd=%d disconnected (EPOLLHUP/ERR)\n", fd);
            sessions.remove(fd);
            ::close(fd);
        }
    );
}

} // namespace exchange
