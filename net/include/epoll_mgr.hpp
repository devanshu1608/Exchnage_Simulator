#pragma once
#include <atomic>
#include <functional>

namespace exchange::net {

using EpollCallback = std::function<void(int fd)>;

class EpollMgr {
public:
    EpollMgr();
    ~EpollMgr();

    // Non-copyable, non-movable
    EpollMgr(const EpollMgr&) = delete;
    EpollMgr& operator=(const EpollMgr&) = delete;
    EpollMgr(EpollMgr&&) = delete;
    EpollMgr& operator=(EpollMgr&&) = delete;

    void add(int fd);

    void remove(int fd);

    void run(EpollCallback onData, EpollCallback onDisconnect);

    void stop();

private:
    int epollFd;
    std::atomic<bool> running{false};

    static constexpr int MAX_EVENTS = 64;
};

} // namespace exchange::net