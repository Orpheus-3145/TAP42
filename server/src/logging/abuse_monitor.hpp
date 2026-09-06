#pragma once

#include <chrono>
#include <deque>
#include <string>

// Deliberately minimal abuse detection: logs a WARN when a threshold is
// exceeded, does not block or disconnect the client.

// Per-SESSION command counter. Not thread-safe on its own, but that's fine:
// it's meant to live inside a Session and is only touched by that
// connection's owning thread (the only one that calls handle_command for
// that session).
class CommandRateTracker {
public:
    // Records a command now and returns true if the rate exceeds the flood
    // threshold (>10 commands/second).
    bool record_and_check_flood();

private:
    std::deque<std::chrono::steady_clock::time_point> timestamps_;
};

// Records a new connection from `ip` and returns true if that IP has opened
// too many connections in a short time (>5 connections/10s).
// Thread-safe: called from different threads (one per accept).
bool record_connection_and_check_rapid_reconnect(const std::string& ip);
