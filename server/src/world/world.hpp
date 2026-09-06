#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct Room {
    std::string id;
    std::string name;
    std::string description;
    std::unordered_map<std::string, std::string> exits; // direction -> room id
    std::vector<std::string> item_instance_ids;
    std::vector<std::string> npc_ids;
};

struct Item {
    std::string id;
    std::string name;
    std::string description;
};

struct Npc {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> dialogue;
    size_t dialogue_index = 0; // index of the next dialogue line to show (cyclic TALK)
    int hp = 0;
    int max_hp = 0;
};

enum class QuestType { Fetch, Defeat };

struct Quest {
    std::string id;
    std::string name;
    std::string description;
    QuestType type;
    std::string target_id;      // item id (Fetch) or npc id (Defeat)
    std::string reward_item_id; // empty = no reward
};

struct PlayerState {
    std::string player_id;
    std::string current_room;
    std::vector<std::string> inventory; // item instance ids
    int hp = 100;
    int max_hp = 100;
    std::string last_target; // last npc_id attacked, used by STATUS
    std::unordered_map<std::string, std::string> quest_status; // quest_id -> status
};

// Shared world state. A singleton because there's a single world for the
// whole server process. `mutex` protects all the maps below: hold it for
// the entire duration of game logic, and only that — never during
// send()/recv().
class World {
public:
    std::mutex mutex;

    std::unordered_map<std::string, Room> rooms;
    std::unordered_map<std::string, Item> items;
    std::unordered_map<std::string, Npc> npcs;
    std::unordered_map<std::string, PlayerState> players; // player_id -> state
    std::unordered_map<std::string, Quest> quests;

    // Groups: player_group[player] = group_id ("" = no group);
    // groups[group_id] = members; pending_invites[invited] = inviter.
    std::unordered_map<std::string, std::string> player_group;
    std::unordered_map<std::string, std::vector<std::string>> groups;
    std::unordered_map<std::string, std::string> pending_invites;

    static World& instance();

private:
    World() = default;
};
