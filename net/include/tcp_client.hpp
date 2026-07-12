#pragma once

/// @file tcp_client.hpp
/// @brief Connects to a remote TCP server and returns a TcpSocket.
///
/// Pure socket wrapper — no business logic.

#include <tcp_socket.hpp>

#include <string>
#include <cstdint>

namespace exchange::net {

class TcpClient {
public:
    /// Blocking connect to host:port. Returns a connected TcpSocket.
    /// Throws std::runtime_error on failure.
    static TcpSocket connect(const std::string& host, uint16_t port);
};

} // namespace exchange::net
