#include "commands/combat_commands.hpp"

#include <algorithm>
#include <random>
#include <sstream>

#include "commands/common.hpp"
#include "commands/quest_commands.hpp"
#include "logging/logger.hpp"

namespace {

constexpr int PLAYER_DAMAGE_MIN = 10;
constexpr int PLAYER_DAMAGE_MAX = 15;
constexpr int NPC_COUNTER_DAMAGE_MIN = 3;
constexpr int NPC_COUNTER_DAMAGE_MAX = 8;
constexpr int RESPAWN_HP = 50;
const std::string SAFE_RESPAWN_ROOM = "loc.start";

int roll_damage(int min_v, int max_v) {
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(min_v, max_v);
    return dist(rng);
}

} // namespace

// Design choice: combat is resolved atomically for each ATTACK command (the
// player's hit plus any NPC counterattack in a single execution), not as a
// "turn-based" state waiting on a second command — simpler to make
// thread-safe and sufficient for this project's scope.
// DEFEND/FLEE are not implemented: left as an open point in PROTOCOL.md,
// not required by the RFC's base command set.
void cmd_attack(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    if (args.empty()) {
        send_line(*session, "ERR ERR_BAD_ARGS ATTACK requires a target");
        return;
    }
    std::string ref = join_from(args, 0);

    auto& world = World::instance();
    std::string response;
    std::string npc_id, room_id;
    int damage_dealt = 0, target_hp = 0, counter_damage = 0, player_hp = 0;
    bool npc_died = false, player_died = false;
    std::string old_room, new_room;

    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto& player = world.players.at(session->player_id);
        if (player.hp <= 0) {
            response = "ERR ERR_DEAD you are down, cannot attack";
        } else {
            auto& room = world.rooms.at(player.current_room);
            npc_id = resolve_npc_ref_locked(room.npc_ids, ref);
            if (npc_id.empty()) {
                response = "ERR ERR_TARGET_NOT_FOUND " + ref;
            } else {
                room_id = room.id;
                auto& npc = world.npcs.at(npc_id);

                damage_dealt = roll_damage(PLAYER_DAMAGE_MIN, PLAYER_DAMAGE_MAX);
                npc.hp = std::max(0, npc.hp - damage_dealt);
                target_hp = npc.hp;
                npc_died = npc.hp <= 0;

                if (npc_died) {
                    room.npc_ids.erase(std::remove(room.npc_ids.begin(), room.npc_ids.end(), npc_id),
                                        room.npc_ids.end());
                } else {
                    counter_damage = roll_damage(NPC_COUNTER_DAMAGE_MIN, NPC_COUNTER_DAMAGE_MAX);
                    player.hp = std::max(0, player.hp - counter_damage);
                }
                player.last_target = npc_id;
                player_hp = player.hp;
                player_died = player.hp <= 0;

                if (player_died) {
                    old_room = player.current_room;
                    new_room = SAFE_RESPAWN_ROOM;
                    player.hp = RESPAWN_HP;
                    player.current_room = new_room;
                    session->current_room = new_room;
                    player_hp = player.hp;
                }

                std::ostringstream oss;
                oss << "OK {\"target\":\"" << npc_id << "\",\"damage_dealt\":" << damage_dealt
                    << ",\"target_hp\":" << target_hp << ",\"counter_damage\":" << counter_damage
                    << ",\"player_hp\":" << player_hp << ",\"respawned\":" << (player_died ? "true" : "false")
                    << "}";
                response = oss.str();
            }
        }
    }

    send_line(*session, response);
    if (!room_id.empty()) {
        broadcast_to_room(room_id, session->player_id,
                           "EVT ROOM COMBAT " + session->player_id + " " + npc_id + " " +
                               std::to_string(damage_dealt) + " " + std::to_string(target_hp));
        log_info("combat_attack", {{"player", session->player_id},
                                    {"target", npc_id},
                                    {"damage", std::to_string(damage_dealt)},
                                    {"target_hp", std::to_string(target_hp)}});
        if (npc_died) {
            broadcast_to_room(room_id, "", "EVT ROOM COMBAT_DEATH " + npc_id);
            log_info("npc_defeated", {{"player", session->player_id}, {"npc", npc_id}});
            on_npc_defeated(session->player_id, npc_id);
        }
    }
    if (player_died) {
        broadcast_to_room(old_room, session->player_id, "EVT ROOM COMBAT_DEATH " + session->player_id);
        broadcast_to_room(old_room, session->player_id, "EVT ROOM PRESENCE LEAVE " + session->player_id);
        broadcast_to_room(new_room, session->player_id, "EVT ROOM PRESENCE ENTER " + session->player_id);
        log_info("player_respawned",
                 {{"player", session->player_id}, {"room", new_room}, {"hp", std::to_string(player_hp)}});
    }
}

void cmd_status(const std::shared_ptr<Session>& session) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    auto& world = World::instance();
    std::ostringstream oss;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto& player = world.players.at(session->player_id);
        bool in_combat = false;
        if (!player.last_target.empty()) {
            auto npc_it = world.npcs.find(player.last_target);
            in_combat = npc_it != world.npcs.end() && npc_it->second.hp > 0;
        }
        oss << "OK {\"hp\":" << player.hp << ",\"max_hp\":" << player.max_hp << ",\"in_combat\":"
            << (in_combat ? "true" : "false") << ",\"target\":"
            << (in_combat ? "\"" + player.last_target + "\"" : "null") << "}";
    }
    send_line(*session, oss.str());
}
