// A lookup table of who's currently connected, keyed by player name. Every
// time we need to push an event at someone, this is how we find their socket.
#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "network/session.hpp"

// Global registry of connected sessions (player_id -> Session), used to
// broadcast events (CHAT GLOBAL, presence, combat, etc).
class SessionRegistry {
public:
    static SessionRegistry& instance();

    void add(const std::string& player_id, std::shared_ptr<Session> session);
    void remove(const std::string& player_id);
    std::shared_ptr<Session> get(const std::string& player_id);
    std::vector<std::shared_ptr<Session>> all();
    size_t count();

private:
    SessionRegistry() = default;
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
};
