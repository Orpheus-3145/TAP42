// Damage rolls, respawn, the lot - see the design-choice note further down
// for why this is atomic-per-command instead of turn-based.
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
constexpr double PLAYER_HIT_CHANCE = 0.85; // player is the hero, missing 15% of swings feels fair
constexpr double NPC_HIT_CHANCE = 0.70;    // counterattacks miss a bit more often
constexpr int RESPAWN_HP = 50;
const std::string SAFE_RESPAWN_ROOM = "loc.start";

std::mt19937& rng() {
    thread_local std::mt19937 generator{std::random_device{}()};
    return generator;
}

int roll_damage(int min_v, int max_v) {
    std::uniform_int_distribution<int> dist(min_v, max_v);
    return dist(rng());
}

bool roll_hit(double hit_chance) {
    std::bernoulli_distribution dist(hit_chance);
    return dist(rng());
}

} // namespace

// Design choice: combat is resolved atomically for each ATTACK command (the
// player's hit plus any NPC counterattack in a single execution), not as a
// "turn-based" state waiting on a second command — simpler to make
// thread-safe and sufficient for this project's scope.
// Both sides roll separately to hit before rolling damage - a miss deals 0
// and skips the death check on that side, but doesn't cancel the other
// side's own roll (missing your swing doesn't stop the NPC hitting back).
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
    bool player_hit = false, npc_countered = false, npc_died = false, player_died = false;
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

                player_hit = roll_hit(PLAYER_HIT_CHANCE);
                damage_dealt = player_hit ? roll_damage(PLAYER_DAMAGE_MIN, PLAYER_DAMAGE_MAX) : 0;
                npc.hp = std::max(0, npc.hp - damage_dealt);
                target_hp = npc.hp;
                npc_died = npc.hp <= 0;

                if (npc_died) {
                    room.npc_ids.erase(std::remove(room.npc_ids.begin(), room.npc_ids.end(), npc_id),
                                        room.npc_ids.end());
                } else {
                    // The NPC still gets to swing back even if the player's own
                    // attack missed - a miss doesn't stun it, it just didn't land.
                    npc_countered = roll_hit(NPC_HIT_CHANCE);
                    counter_damage = npc_countered ? roll_damage(npc.counter_damage_min, npc.counter_damage_max) : 0;
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
                oss << "OK {\"target\":\"" << npc_id << "\",\"hit\":" << (player_hit ? "true" : "false")
                    << ",\"damage_dealt\":" << damage_dealt << ",\"target_hp\":" << target_hp
                    << ",\"countered\":" << (npc_countered ? "true" : "false")
                    << ",\"counter_damage\":" << counter_damage << ",\"player_hp\":" << player_hp
                    << ",\"respawned\":" << (player_died ? "true" : "false") << "}";
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
