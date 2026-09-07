// Nothing clever - a mutex and a map. It just needs to be correct.
#include "network/registry.hpp"

SessionRegistry& SessionRegistry::instance() {
    static SessionRegistry registry;
    return registry;
}

void SessionRegistry::add(const std::string& player_id, std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[player_id] = std::move(session);
}

void SessionRegistry::remove(const std::string& player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(player_id);
}

std::shared_ptr<Session> SessionRegistry::get(const std::string& player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(player_id);
    return it == sessions_.end() ? nullptr : it->second;
}

std::vector<std::shared_ptr<Session>> SessionRegistry::all() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Session>> out;
    out.reserve(sessions_.size());
    for (auto& [id, session] : sessions_) out.push_back(session);
    return out;
}

size_t SessionRegistry::count() {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}
