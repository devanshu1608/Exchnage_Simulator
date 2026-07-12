#pragma once
#include <cstddef>
#include <sys/types.h>

namespace exchange::net {

class TcpSocket {
public:
    TcpSocket();
    explicit TcpSocket(int fd);

    ~TcpSocket();

    // Move only — no copies
    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    int fd() const { return mFd; }

    bool valid() const { return mFd >= 0; }

    void setNonBlocking();

    void setReuseAddr();

    ssize_t sendRaw(const char* msg, std::size_t msgLen);

    ssize_t recvRaw(char* msg, std::size_t msgLen);

    int release();

    void close();

private:
    int mFd;
};

} // namespace exchange::net
