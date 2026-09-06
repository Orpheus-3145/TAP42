#include "logging/abuse_monitor.hpp"

#include <mutex>
#include <unordered_map>

namespace {

constexpr int COMMAND_FLOOD_THRESHOLD = 10; // commands
constexpr auto COMMAND_FLOOD_WINDOW = std::chrono::seconds(1);
constexpr int RECONNECT_THRESHOLD = 5; // connections
constexpr auto RECONNECT_WINDOW = std::chrono::seconds(10);

std::mutex reconnect_mutex;
std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> reconnect_history;

} // namespace

bool CommandRateTracker::record_and_check_flood() {
    auto now = std::chrono::steady_clock::now();
    timestamps_.push_back(now);
    while (!timestamps_.empty() && now - timestamps_.front() > COMMAND_FLOOD_WINDOW) timestamps_.pop_front();
    return static_cast<int>(timestamps_.size()) > COMMAND_FLOOD_THRESHOLD;
}

bool record_connection_and_check_rapid_reconnect(const std::string& ip) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(reconnect_mutex);
    auto& history = reconnect_history[ip];
    history.push_back(now);
    while (!history.empty() && now - history.front() > RECONNECT_WINDOW) history.pop_front();
    return static_cast<int>(history.size()) > RECONNECT_THRESHOLD;
}
