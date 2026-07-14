#include "app_threads.hpp"
#include <protocol.hpp>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>

namespace exchange {
using namespace exchange::net;

void runEpollThread(net::EpollMgr& epoll, SessionManager& sessions, SPSCQueue<RawMessage, 65536>& rawQueue, std::atomic<bool>& /*gRunning*/) {
    std::printf("starting epoll event loop\n");

    epoll.run(
        // on data callback
        [&](int fd) {
            ClientSession* session = sessions.getSession(fd);
            if (!session) return;

            // read all available data edge triggered
            char buf[4096];
            while (true) {
                ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
                if (n <= 0) {
                    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                        // client disconnected or error
                        std::printf("client fd=%d disconnected\n", fd);
                        epoll.remove(fd);
                        sessions.remove(fd);
                        ::close(fd);
                    }
                    break;
                }

                // append to session recv buffer
                session->recvBuffer.insert(
                    session->recvBuffer.end(), buf, buf + n);
            }

            // frame complete messages and push raw bytes to the queue
            // only reads header length for framing
            while (session->recvBuffer.size() >= sizeof(MsgHeader)) {
                MsgHeader header;
                std::memcpy(&header, session->recvBuffer.data(), sizeof(MsgHeader));

                // wait for complete message
                if (session->recvBuffer.size() < header.length) break;

                // sanity check
                if (header.length > MAX_RAW_MSG_SIZE) {
                    std::fprintf(stderr, "oversized message (%u bytes) from fd=%d dropping\n",
                        header.length, fd);
                    session->recvBuffer.erase(
                        session->recvBuffer.begin(),
                        session->recvBuffer.begin() + header.length);
                    continue;
                }

                // push raw bytes no parsing
                RawMessage raw;
                raw.traderId = session->traderId;
                raw.length   = header.length;
                std::memcpy(raw.data, session->recvBuffer.data(), header.length);
                rawQueue.push(raw);

                // remove processed bytes from the recv buffer
                session->recvBuffer.erase(
                    session->recvBuffer.begin(),
                    session->recvBuffer.begin() + header.length);
            }
        },
        // on disconnect callback
        [&](int fd) {
            std::printf("client fd=%d disconnected with error\n", fd);
            sessions.remove(fd);
            ::close(fd);
        }
    );
}

} // namespace exchange
