#include "app_threads.hpp"
#include <thread>
#include <atomic>
#include <cstdio>
#include <csignal>

using namespace exchange;
using namespace exchange::net;

static std::atomic<bool> gRunning{true};

static void signalHandler(int) {
    gRunning = false;
}

int main() {
    constexpr uint16_t ORDER_PORT = 9001;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::puts("Exchange Server");

    auto rawQueuePtr = std::make_unique<SPSCQueue<RawMessage, 65536>>();
    auto& rawQueue = *rawQueuePtr;
    SessionManager sessions;
    MarketState marketState;
    EpollMgr epoll;

    //accepts incoming connections, adds to epoll
    std::thread acceptThread([&] {
        runAcceptThread(ORDER_PORT, sessions, epoll, gRunning);
    });

    //mathing engine thread
    std::thread matchingThread([&] {
        runMatchingThread(rawQueue, sessions, marketState, gRunning);
    });

    //terminal gui thread
    std::thread guiThread([&] {
        runGuiThread(marketState, gRunning);
    });

    //epoll incoming order requests
    runEpollThread(epoll, sessions, rawQueue, gRunning);


    std::puts("Exchange shutting down");
    gRunning = false;

    matchingThread.join();
    guiThread.join();
    acceptThread.join();

    std::puts("Exchange shutdown complete");
    return 0;
}
