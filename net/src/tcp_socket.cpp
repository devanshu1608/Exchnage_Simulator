#include <tcp_socket.hpp>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <utility>

namespace exchange::net {

TcpSocket::TcpSocket() : mFd(-1) {}

TcpSocket::TcpSocket(int fd) : mFd(fd) {}

TcpSocket::~TcpSocket() {
    close();
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : mFd(other.mFd) {
    other.mFd = -1;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this != &other) {
        close();
        mFd = other.mFd;
        other.mFd = -1;
    }
    return *this;
}

void TcpSocket::setNonBlocking() {
    if (mFd < 0) return;
    int flags = ::fcntl(mFd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(mFd, F_SETFL, flags | O_NONBLOCK);
    }
}

void TcpSocket::setReuseAddr() {
    if (mFd < 0) return;
    int opt = 1;
    ::setsockopt(mFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

ssize_t TcpSocket::sendRaw(const char* msg, std::size_t msgLen) {
    if (mFd < 0) return -1;
    return ::send(mFd, msg, msgLen, MSG_NOSIGNAL);
}

ssize_t TcpSocket::recvRaw(char* msg, std::size_t msgLen) {
    if (mFd < 0) return -1;
    return ::recv(mFd, msg, msgLen, 0);
}

int TcpSocket::release() {
    int fd = mFd;
    mFd = -1;
    return fd;
}

void TcpSocket::close() {
    if (mFd >= 0) {
        ::close(mFd);
        mFd = -1;
    }
}

} // namespace exchange::net
