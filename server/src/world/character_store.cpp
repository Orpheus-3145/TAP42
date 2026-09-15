#include "world/character_store.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>

#include "logging/logger.hpp"
#include "world/json.hpp"

namespace character_store {

namespace {

std::string g_directory = "data/characters"; // overwritten by init()

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

std::string path_for(const std::string& name) { return g_directory + "/" + name + ".json"; }

// One mutex per character name, so a save of "gandalf" never waits on a
// save of "frodo" — only two saves of the SAME character (e.g. the
// player's own MOVE and someone else's kill completing one of their
// quests, landing on two different connection threads at once) serialize
// against each other.
std::mutex& lock_for(const std::string& name) {
    static std::mutex map_mutex;
    static std::unordered_map<std::string, std::unique_ptr<std::mutex>> locks;
    std::lock_guard<std::mutex> guard(map_mutex);
    auto& slot = locks[name];
    if (!slot) slot = std::make_unique<std::mutex>();
    return *slot;
}

} // namespace

void init(const std::string& directory) {
    g_directory = directory;
    std::filesystem::create_directories(g_directory);
}

bool is_valid_name(const std::string& name) {
    if (name.empty() || name.size() > 32) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
    }
    return true;
}

bool exists(const std::string& name) { return std::filesystem::exists(path_for(name)); }

bool load(const std::string& name, PlayerState& out) {
    std::ifstream file(path_for(name), std::ios::binary);
    if (!file) return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();

    JsonValue root;
    std::string error;
    if (!parse_json(buffer.str(), root, error)) {
        log_error("character_load_failed", {{"name", name}, {"error", error}});
        return false;
    }

    out = PlayerState{};
    out.player_id = name;
    out.race = root["race"].as_string();
    out.special_attributes = root["special_attributes"].as_string();
    out.current_room = root["current_room"].as_string();
    out.hp = root["hp"].as_int();
    out.max_hp = root["max_hp"].as_int();
    out.last_target = root["last_target"].as_string();
    for (auto& item : root["inventory"].array_value) out.inventory.push_back(item.as_string());
    for (auto& [quest_id, status] : root["quest_status"].object_value) out.quest_status[quest_id] = status.as_string();
    for (auto& [npc_id, idx] : root["npc_dialogue_progress"].object_value) {
        out.npc_dialogue_progress[npc_id] = static_cast<size_t>(idx.as_int());
    }
    return true;
}

void save(const PlayerState& state) {
    std::ostringstream oss;
    oss << "{\"name\":\"" << json_escape(state.player_id) << "\""
        << ",\"race\":\"" << json_escape(state.race) << "\""
        << ",\"special_attributes\":\"" << json_escape(state.special_attributes) << "\""
        << ",\"current_room\":\"" << state.current_room << "\""
        << ",\"hp\":" << state.hp << ",\"max_hp\":" << state.max_hp
        << ",\"last_target\":\"" << state.last_target << "\""
        << ",\"inventory\":[";
    for (size_t i = 0; i < state.inventory.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << state.inventory[i] << "\"";
    }
    oss << "],\"quest_status\":{";
    bool first = true;
    for (auto& [quest_id, status] : state.quest_status) {
        if (!first) oss << ",";
        oss << "\"" << quest_id << "\":\"" << status << "\"";
        first = false;
    }
    oss << "},\"npc_dialogue_progress\":{";
    first = true;
    for (auto& [npc_id, idx] : state.npc_dialogue_progress) {
        if (!first) oss << ",";
        oss << "\"" << npc_id << "\":" << idx;
        first = false;
    }
    oss << "}}";

    std::lock_guard<std::mutex> guard(lock_for(state.player_id));
    std::ofstream file(path_for(state.player_id), std::ios::binary | std::ios::trunc);
    if (!file) {
        log_error("character_save_failed", {{"name", state.player_id}, {"error", "cannot open file for write"}});
        return;
    }
    file << oss.str();
}

} // namespace character_store
