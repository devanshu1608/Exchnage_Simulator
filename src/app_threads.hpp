#pragma once
#include <tcp_server.hpp>
#include <epoll_mgr.hpp>
#include <spsc_queue.hpp>
#include <order.hpp>
#include <raw_message.hpp>
#include <atomic>

#include "session_manager.hpp"
#include "market_state.hpp"

namespace exchange {

void runAcceptThread(uint16_t port, SessionManager& sessions, net::EpollMgr& epoll, std::atomic<bool>& gRunning);

void runMatchingThread(SPSCQueue<RawMessage, 65536>& rawQueue, SessionManager& sessions, MarketState& marketState, std::atomic<bool>& gRunning);

void runGuiThread(MarketState& marketState, std::atomic<bool>& gRunning);

void runEpollThread(net::EpollMgr& epoll, SessionManager& sessions, SPSCQueue<RawMessage, 65536>& rawQueue, std::atomic<bool>& gRunning);

} // namespace exchange
