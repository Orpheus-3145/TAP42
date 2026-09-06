#pragma once

#include <atomic>
#include <mutex>
#include <string>

#include "logging/abuse_monitor.hpp"
#include "network/platform_socket.hpp"

// State of a client connection. One instance per thread-per-client.
// write_mutex serializes send() calls on this socket: both the owning thread
// (replying OK/ERR to its own command) and threads belonging to OTHER
// clients (notifying this player of an EVT) may write to it.
struct Session {
    net::socket_t socket_fd = net::INVALID_SOCK;
    std::mutex write_mutex;
    std::string player_id;   // empty until CONNECT is received
    std::string current_room;
    std::atomic<bool> connected{true};
    std::string recv_buffer; // partial recv() accumulator, consumed by read_line()
    std::string peer_ip;
    CommandRateTracker rate_tracker; // touched only by the owning thread, see header
};

void send_line(Session& session, const std::string& message);
