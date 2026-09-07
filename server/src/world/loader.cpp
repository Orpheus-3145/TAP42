// A broken world file should fail loudly at startup, not cause weird bugs
// three commands into a play session - hence all the validation below.
#include "world/loader.hpp"

#include <fstream>
#include <set>
#include <sstream>

#include "logging/logger.hpp"
#include "world/json.hpp"
#include "world/world.hpp"

namespace {

bool read_file(const std::string& path, std::string& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open file";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

bool parse_quest_type(const std::string& type_str, QuestType& out) {
    if (type_str == "fetch") {
        out = QuestType::Fetch;
        return true;
    }
    if (type_str == "defeat") {
        out = QuestType::Defeat;
        return true;
    }
    return false;
}

} // namespace

bool load_world(const std::string& path) {
    std::string raw, error;
    if (!read_file(path, raw, error)) {
        log_error("world_load_failed", {{"path", path}, {"error", error}});
        return false;
    }

    JsonValue root;
    if (!parse_json(raw, root, error)) {
        log_error("world_parse_failed", {{"path", path}, {"error", error}});
        return false;
    }
    if (!root.is_object()) {
        log_error("world_validation_failed", {{"error", "root must be a JSON object"}});
        return false;
    }

    auto& world = World::instance();
    std::lock_guard<std::mutex> lock(world.mutex);
    world.rooms.clear();
    world.items.clear();
    world.npcs.clear();
    world.quests.clear();

    // --- Items ---
    for (auto& [item_id, item_obj] : root["items"].object_value) {
        Item item;
        item.id = item_id;
        item.name = item_obj["name"].as_string();
        item.description = item_obj["description"].as_string();
        world.items[item_id] = item;
    }

    // --- NPCs ---
    for (auto& [npc_id, npc_obj] : root["npcs"].object_value) {
        Npc npc;
        npc.id = npc_id;
        npc.name = npc_obj["name"].as_string();
        npc.description = npc_obj["description"].as_string();
        for (auto& line : npc_obj["dialogue"].array_value) npc.dialogue.push_back(line.as_string());
        npc.hp = npc_obj["hp"].as_int();
        npc.max_hp = npc.hp;
        world.npcs[npc_id] = npc;
    }

    // --- Rooms ---
    std::set<std::string> placed_items; // to validate instance uniqueness
    for (auto& [room_id, room_obj] : root["rooms"].object_value) {
        Room room;
        room.id = room_id;
        room.name = room_obj["name"].as_string();
        room.description = room_obj["description"].as_string();
        for (auto& [dir, target] : room_obj["exits"].object_value) room.exits[dir] = target.as_string();
        for (auto& item_ref : room_obj["items"].array_value) room.item_instance_ids.push_back(item_ref.as_string());
        for (auto& npc_ref : room_obj["npcs"].array_value) room.npc_ids.push_back(npc_ref.as_string());
        world.rooms[room_id] = room;
    }

    // --- Quests ---
    for (auto& [quest_id, quest_obj] : root["quests"].object_value) {
        Quest quest;
        quest.id = quest_id;
        quest.name = quest_obj["name"].as_string();
        quest.description = quest_obj["description"].as_string();
        std::string type_str = quest_obj["type"].as_string();
        if (!parse_quest_type(type_str, quest.type)) {
            log_error("world_validation_failed",
                      {{"quest", quest_id}, {"error", "unknown quest type '" + type_str + "'"}});
            return false;
        }
        quest.target_id = quest_obj["target_id"].as_string();
        quest.reward_item_id = quest_obj["reward_item_id"].as_string();
        world.quests[quest_id] = quest;
    }

    // --- Referential validation ---
    for (auto& [room_id, room] : world.rooms) {
        for (auto& [dir, target] : room.exits) {
            if (!world.rooms.count(target)) {
                log_error("world_validation_failed", {{"room", room_id},
                                                        {"exit", dir},
                                                        {"error", "exit target '" + target + "' does not exist"}});
                return false;
            }
        }
        for (auto& item_id : room.item_instance_ids) {
            if (!world.items.count(item_id)) {
                log_error("world_validation_failed",
                          {{"room", room_id}, {"error", "unknown item '" + item_id + "'"}});
                return false;
            }
            if (!placed_items.insert(item_id).second) {
                log_error("world_validation_failed",
                          {{"room", room_id}, {"error", "item '" + item_id + "' placed in more than one room"}});
                return false;
            }
        }
        for (const auto& npc_id : room.npc_ids) {
            if (!world.npcs.count(npc_id)) {
                log_error("world_validation_failed",
                          {{"room", room_id}, {"error", "unknown npc '" + npc_id + "'"}});
                return false;
            }
        }
    }
    for (auto& [quest_id, quest] : world.quests) {
        bool target_ok = quest.type == QuestType::Fetch ? world.items.count(quest.target_id) > 0
                                                          : world.npcs.count(quest.target_id) > 0;
        if (!target_ok) {
            log_error("world_validation_failed",
                      {{"quest", quest_id}, {"error", "target '" + quest.target_id + "' does not exist"}});
            return false;
        }
        if (!quest.reward_item_id.empty() && !world.items.count(quest.reward_item_id)) {
            log_error("world_validation_failed",
                      {{"quest", quest_id}, {"error", "reward item '" + quest.reward_item_id + "' does not exist"}});
            return false;
        }
    }
    if (!world.rooms.count("loc.start")) {
        log_error("world_validation_failed", {{"error", "missing required spawn room 'loc.start'"}});
        return false;
    }

    log_info("world_loaded", {{"path", path},
                               {"rooms", std::to_string(world.rooms.size())},
                               {"items", std::to_string(world.items.size())},
                               {"npcs", std::to_string(world.npcs.size())},
                               {"quests", std::to_string(world.quests.size())}});
    return true;
}
