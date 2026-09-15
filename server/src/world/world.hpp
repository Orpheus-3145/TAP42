// The whole game state lives here - rooms, items, NPCs, players, quests,
// groups - all sitting behind one mutex. Nothing fancy, just correct.
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

enum class ItemEffect { None, Heal, Damage };

struct Item {
    std::string id;
    std::string name;
    std::string description;
    ItemEffect effect = ItemEffect::None;
    int effect_amount = 0; // hp healed (Heal) or damage dealt (Damage)
};

struct Npc {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> dialogue;
    int hp = 0;
    int max_hp = 0;
    // Per-NPC counter-attack range; a world file that doesn't set these gets
    // the old one-size-fits-all values. min == max gives a fixed hit
    // (e.g. the bartender's flat 100) instead of a rolled range.
    int counter_damage_min = 3;
    int counter_damage_max = 8;

    // Extra lines appended to the TALK cycle once a player has "completed"
    // status on every quest listed in unlock_quest_ids. Either left empty
    // for an NPC with nothing to unlock.
    std::vector<std::string> bonus_dialogue;
    std::vector<std::string> unlock_quest_ids;
};

enum class QuestType { Fetch, Defeat };

struct Quest {
    std::string id;
    std::string name;
    std::string description;
    QuestType type = QuestType::Fetch;
    std::string target_id;           // Fetch: the item to bring back
    std::vector<std::string> target_ids; // Defeat: every npc that must die; a
                                          // single-boss quest just lists one
    std::string reward_item_id; // empty = no reward
};

struct PlayerState {
    std::string player_id;

    // Chosen at character creation, free text within a fixed pair of slots.
    // Purely cosmetic: nothing in game logic ever reads these to change a
    // stat or a roll, they only exist for the client to show back to the player.
    std::string race;
    std::string special_attributes;

    std::string current_room;
    std::vector<std::string> inventory; // item instance ids
    int hp = 100;
    int max_hp = 100;
    std::string last_target; // last npc_id attacked, used by STATUS
    std::unordered_map<std::string, std::string> quest_status; // quest_id -> status

    // Per-player dialogue progress: npc_id -> index of the next line to show.
    // Deliberately NOT on Npc — each player cycles an NPC's dialogue on
    // their own, independent of what everyone else has heard from it.
    std::unordered_map<std::string, size_t> npc_dialogue_progress;
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
