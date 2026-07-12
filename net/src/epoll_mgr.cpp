#include <cstdio>
#include <epoll_mgr.hpp>
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>

namespace exchange::net {

EpollMgr::EpollMgr() {
  epollFd = ::epoll_create1(0);
  if (epollFd < 0) {
    throw std::runtime_error("EpollMgr: epoll_create1 failed");
  }
}

EpollMgr::~EpollMgr() {
  if (epollFd >= 0) {
    ::close(epollFd);
  }
}

void EpollMgr::add(int fd) {
  struct epoll_event ev{};
  ev.events = EPOLLIN | EPOLLET; // Edge-triggered, read events
  ev.data.fd = fd;

  if (::epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev) < 0) {
    std::fprintf(stderr, "[EpollMgr] Failed to add fd=%d\n", fd);
  }
}

void EpollMgr::remove(int fd) {
  ::epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
}

void EpollMgr::run(EpollCallback onData, EpollCallback onDisconnect) {
  running = true;
  struct epoll_event events[MAX_EVENTS];

  while (running) {
    int n = ::epoll_wait(epollFd, events, MAX_EVENTS, 100);

    for (int i = 0; i < n; ++i) {
      int fd = events[i].data.fd;
      uint32_t ev = events[i].events;

      if (ev & (EPOLLHUP | EPOLLERR)) {
        if (onDisconnect) {
          onDisconnect(fd);
        }
      } else if (ev & EPOLLIN) {
        if (onData) {
          onData(fd);
        }
      }
    }
  }
}

void EpollMgr::stop() { running = false; }

} // namespace exchange::net
