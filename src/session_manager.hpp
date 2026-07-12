#pragma once

/// @file session_manager.hpp
/// @brief Thread-safe mapping of fd ↔ TraderId with per-fd receive buffers.
///
/// Shared between:
///   - Accept threads (write: add sessions after login)
///   - Main thread   (read: recv buffers + trader lookup in epoll callback)
///   - Matching thread (read: reverse lookup fd from trader_id, get all fds for multicast)

#include <types.hpp>

#include <unordered_map>
#include <vector>
#include <mutex>

namespace exchange {

/// Per-client session state.
struct ClientSession {
    int fd;
    TraderId traderId;
    std::vector<char> recvBuffer;  // For TCP message framing (partial reads)
};

class SessionManager {
public:
    /// Register a new session. Called by accept/login threads.
    void add(int fd, TraderId traderId) {
        std::lock_guard<std::mutex> lock(mtx);
        sessions[fd] = ClientSession{fd, traderId, {}};
        traderToFd[traderId] = fd;
    }

    /// Remove a session. Called by main thread on disconnect.
    void remove(int fd) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = sessions.find(fd);
        if (it != sessions.end()) {
            traderToFd.erase(it->second.traderId);
            sessions.erase(it);
        }
    }

    /// Get session by fd. Returns nullptr if not found.
    /// The recvBuffer is only accessed by the main thread (epoll callback),
    /// so no separate synchronization needed for buffer operations.
    ClientSession* getSession(int fd) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = sessions.find(fd);
        return (it != sessions.end()) ? &it->second : nullptr;
    }

    /// Get traderId for a given fd. Returns 0 if not found.
    TraderId getTraderId(int fd) const {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = sessions.find(fd);
        return (it != sessions.end()) ? it->second.traderId : 0;
    }

    /// Get fd for a given traderId. Returns -1 if not found.
    int getFd(TraderId id) const {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = traderToFd.find(id);
        return (it != traderToFd.end()) ? it->second : -1;
    }

    /// Get all connected fds. Used for market data multicast.
    std::vector<int> getAllFds() const {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<int> fds;
        fds.reserve(sessions.size());
        for (const auto& [fd, _] : sessions) {
            fds.push_back(fd);
        }
        return fds;
    }

private:
    std::unordered_map<int, ClientSession> sessions;
    std::unordered_map<TraderId, int> traderToFd;
    mutable std::mutex mtx;
};

} // namespace exchange
