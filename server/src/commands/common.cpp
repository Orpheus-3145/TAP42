// broadcast_to_room/broadcast_to_group always release the world lock before
// touching a socket - never hold a mutex while doing network I/O.
#include "commands/common.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "network/registry.hpp"
#include "network/session.hpp"

std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

std::string join_from(const std::vector<std::string>& tokens, size_t start) {
    std::string out;
    for (size_t i = start; i < tokens.size(); ++i) {
        if (i > start) out += " ";
        out += tokens[i];
    }
    return out;
}

std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        if (c == '\n') { out += "\\n"; continue; }
        out.push_back(c);
    }
    return out;
}

void broadcast_to_room(const std::string& room_id, const std::string& except_player,
                        const std::string& message) {
    auto& world = World::instance();
    std::vector<std::string> targets;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        for (auto& [pid, player] : world.players) {
            if (player.current_room == room_id && pid != except_player) targets.push_back(pid);
        }
    }
    for (const auto& pid : targets) {
        auto session = SessionRegistry::instance().get(pid);
        if (session) send_line(*session, message);
    }
}

void broadcast_to_group(const std::string& group_id, const std::string& except_player,
                         const std::string& message) {
    if (group_id.empty()) return;
    auto& world = World::instance();
    std::vector<std::string> targets;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto it = world.groups.find(group_id);
        if (it != world.groups.end()) {
            for (const auto& pid : it->second) {
                if (pid != except_player) targets.push_back(pid);
            }
        }
    }
    for (const auto& pid : targets) {
        auto session = SessionRegistry::instance().get(pid);
        if (session) send_line(*session, message);
    }
}

std::string serialize_room_locked(const Room& room) {
    std::ostringstream oss;
    oss << "{\"room\":{\"id\":\"" << room.id << "\",\"name\":\"" << json_escape(room.name)
        << "\",\"description\":\"" << json_escape(room.description) << "\",\"exits\":{";
    bool first = true;
    for (auto& [dir, dest] : room.exits) {
        if (!first) oss << ",";
        oss << "\"" << dir << "\":\"" << dest << "\"";
        first = false;
    }
    oss << "}}";

    auto& world = World::instance();
    oss << ",\"players\":[";
    first = true;
    for (auto& [pid, player] : world.players) {
        if (player.current_room != room.id) continue;
        if (!first) oss << ",";
        oss << "\"" << pid << "\"";
        first = false;
    }
    oss << "],\"items\":[";
    first = true;
    for (const auto& item_id : room.item_instance_ids) {
        if (!first) oss << ",";
        oss << "\"" << item_id << "\"";
        first = false;
    }
    oss << "],\"npcs\":[";
    first = true;
    for (const auto& npc_id : room.npc_ids) {
        if (!first) oss << ",";
        oss << "\"" << npc_id << "\"";
        first = false;
    }
    oss << "]}";
    return oss.str();
}

std::string resolve_item_ref_locked(const std::vector<std::string>& candidate_ids, const std::string& ref) {
    for (const auto& id : candidate_ids) {
        if (id == ref) return id;
    }
    auto& world = World::instance();
    std::string ref_lower = to_lower(ref);
    for (auto& id : candidate_ids) {
        auto it = world.items.find(id);
        if (it != world.items.end() && to_lower(it->second.name) == ref_lower) return id;
    }
    return "";
}

std::string resolve_npc_ref_locked(const std::vector<std::string>& candidate_ids, const std::string& ref) {
    for (const auto& id : candidate_ids) {
        if (id == ref) return id;
    }
    auto& world = World::instance();
    std::string ref_lower = to_lower(ref);
    for (auto& id : candidate_ids) {
        auto it = world.npcs.find(id);
        if (it != world.npcs.end() && to_lower(it->second.name) == ref_lower) return id;
    }
    return "";
}
