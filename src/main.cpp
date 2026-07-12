/// @file main.cpp
/// @brief Exchange server entry point — wires all components together.
///
/// Thread model:
///   Thread 1 (Accept):   TcpServer accepts clients, login threads handle auth
///   Thread 2 (Main):     EpollMgr reads client data, parses, pushes to SPSC queue
///   Thread 3 (Matching): Pops from queue, matches, sends responses
///   Thread 4 (HTTP):     Serves market data GUI dashboard

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

    std::puts("=== Exchange Simulator Server ===");

    // ── Shared state (all on stack, passed by reference) ──
    auto rawQueuePtr = std::make_unique<SPSCQueue<RawMessage, 65536>>();
    auto& rawQueue = *rawQueuePtr;
    SessionManager sessions;
    MarketState marketState;
    EpollMgr epoll;

    // ── Thread 1: Accept + Login ──
    std::thread acceptThread([&] {
        runAcceptThread(ORDER_PORT, sessions, epoll, gRunning);
    });

    // ── Thread 3: Matching Engine ──
    std::thread matchingThread([&] {
        runMatchingThread(rawQueue, sessions, marketState, gRunning);
    });

    // ── Thread 4: Terminal GUI ──
    std::thread guiThread([&] {
        runGuiThread(marketState, gRunning);
    });

    // ── Thread 2: Main Thread — Epoll Event Loop ──
    runEpollThread(epoll, sessions, rawQueue, gRunning);

    // ── Shutdown ──
    std::puts("[Main] Shutting down...");
    gRunning = false;

    // Note: acceptThread runs TcpServer::start() which blocks on accept().
    // It will be interrupted when the process exits. For clean shutdown,
    // TcpServer::stop() closes the listen socket to unblock accept().

    matchingThread.join();
    guiThread.join();
    // acceptThread is blocked on accept(); we detach it for clean exit
    acceptThread.detach();

    std::puts("[Main] Shutdown complete.");
    return 0;
}
