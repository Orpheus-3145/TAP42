// Straightforward: resolve the reference, move the id between the two
// vectors, tell the quest system if a fetch quest just got satisfied.
#include "commands/item_commands.hpp"

#include <algorithm>
#include <sstream>

#include "commands/common.hpp"
#include "commands/quest_commands.hpp"
#include "logging/logger.hpp"
#include "world/character_store.hpp"

void cmd_take(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    if (args.empty()) {
        send_line(*session, "ERR ERR_BAD_ARGS TAKE requires an item");
        return;
    }
    std::string ref = join_from(args, 0);

    auto& world = World::instance();
    std::string response, taken_id;
    PlayerState taker;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto& player = world.players.at(session->player_id);
        auto& room = world.rooms.at(player.current_room);
        std::string item_id = resolve_item_ref_locked(room.item_instance_ids, ref);
        if (item_id.empty()) {
            response = "ERR ERR_ITEM_NOT_FOUND " + ref;
        } else {
            room.item_instance_ids.erase(
                std::remove(room.item_instance_ids.begin(), room.item_instance_ids.end(), item_id),
                room.item_instance_ids.end());
            player.inventory.push_back(item_id);
            response = "OK taken=" + item_id;
            taken_id = item_id;
            taker = player;
        }
    }
    send_line(*session, response);
    if (!taken_id.empty()) {
        character_store::save(taker);
        log_info("item_taken", {{"player", session->player_id}, {"item", taken_id}});
        on_item_taken(session->player_id, taken_id);
    }
}

void cmd_drop(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    if (args.empty()) {
        send_line(*session, "ERR ERR_BAD_ARGS DROP requires an item");
        return;
    }
    std::string ref = join_from(args, 0);

    auto& world = World::instance();
    std::string response, dropped_id;
    PlayerState dropper;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto& player = world.players.at(session->player_id);
        auto& room = world.rooms.at(player.current_room);
        std::string item_id = resolve_item_ref_locked(player.inventory, ref);
        if (item_id.empty()) {
            response = "ERR ERR_ITEM_NOT_FOUND " + ref;
        } else {
            player.inventory.erase(std::remove(player.inventory.begin(), player.inventory.end(), item_id),
                                    player.inventory.end());
            room.item_instance_ids.push_back(item_id);
            response = "OK dropped=" + item_id;
            dropped_id = item_id;
            dropper = player;
        }
    }
    send_line(*session, response);
    if (!dropped_id.empty()) {
        character_store::save(dropper);
        log_info("item_dropped", {{"player", session->player_id}, {"item", dropped_id}});
    }
}

// USE resolves the item against the inventory (not the room, unlike TAKE/DROP
// which resolve against different lists) — you can only use what you're
// carrying. Heal and damage effects share the lock/response/broadcast shape
// of the rest of the item and combat commands: mutate under the lock, then
// send + broadcast after releasing it.
void cmd_use(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    if (args.empty()) {
        send_line(*session, "ERR ERR_BAD_ARGS USE requires an item");
        return;
    }
    std::string item_ref = args[0];
    std::string target_ref = args.size() > 1 ? join_from(args, 1) : "";

    auto& world = World::instance();
    std::string response, room_id, used_item_id, npc_id;
    int heal_amount = 0, damage_dealt = 0, target_hp = 0;
    bool is_heal = false, is_damage = false, npc_died = false;
    PlayerState user;
    bool user_mutated = false;

    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto& player = world.players.at(session->player_id);
        if (player.hp <= 0) {
            response = "ERR ERR_DEAD you are down, cannot use items";
        } else {
            std::string item_id = resolve_item_ref_locked(player.inventory, item_ref);
            if (item_id.empty()) {
                response = "ERR ERR_ITEM_NOT_FOUND " + item_ref;
            } else {
                const auto& item = world.items.at(item_id);
                room_id = player.current_room;

                if (item.effect == ItemEffect::Heal) {
                    is_heal = true;
                    used_item_id = item_id;
                    player.inventory.erase(std::remove(player.inventory.begin(), player.inventory.end(), item_id),
                                            player.inventory.end());
                    player.hp = std::min(player.max_hp, player.hp + item.effect_amount);
                    heal_amount = item.effect_amount;
                    response = "OK healed=" + std::to_string(heal_amount) + " hp=" + std::to_string(player.hp);
                } else if (item.effect == ItemEffect::Damage) {
                    if (target_ref.empty()) {
                        response = "ERR ERR_BAD_ARGS USE " + item_ref + " requires a target";
                    } else {
                        auto& room = world.rooms.at(player.current_room);
                        npc_id = resolve_npc_ref_locked(room.npc_ids, target_ref);
                        if (npc_id.empty()) {
                            response = "ERR ERR_TARGET_NOT_FOUND " + target_ref;
                        } else {
                            is_damage = true;
                            used_item_id = item_id;
                            player.inventory.erase(
                                std::remove(player.inventory.begin(), player.inventory.end(), item_id),
                                player.inventory.end());
                            auto& npc = world.npcs.at(npc_id);
                            damage_dealt = item.effect_amount;
                            npc.hp = std::max(0, npc.hp - damage_dealt);
                            target_hp = npc.hp;
                            npc_died = npc.hp <= 0;
                            if (npc_died) {
                                room.npc_ids.erase(std::remove(room.npc_ids.begin(), room.npc_ids.end(), npc_id),
                                                    room.npc_ids.end());
                            }
                            std::ostringstream oss;
                            oss << "OK {\"target\":\"" << npc_id << "\",\"damage_dealt\":" << damage_dealt
                                << ",\"target_hp\":" << target_hp << "}";
                            response = oss.str();
                        }
                    }
                } else {
                    response = "ERR ERR_BAD_ARGS " + item_ref + " cannot be used";
                }
                if (is_heal || is_damage) {
                    user = player;
                    user_mutated = true;
                }
            }
        }
    }

    send_line(*session, response);
    if (user_mutated) character_store::save(user);
    if (is_heal) {
        log_info("item_used", {{"player", session->player_id},
                                {"item", used_item_id},
                                {"effect", "heal"},
                                {"amount", std::to_string(heal_amount)}});
        broadcast_to_room(room_id, session->player_id, "EVT ROOM ITEM_USE " + session->player_id + " " + used_item_id);
    } else if (is_damage) {
        log_info("item_used", {{"player", session->player_id},
                                {"item", used_item_id},
                                {"effect", "damage"},
                                {"target", npc_id},
                                {"damage", std::to_string(damage_dealt)}});
        broadcast_to_room(room_id, session->player_id,
                           "EVT ROOM COMBAT " + session->player_id + " " + npc_id + " " +
                               std::to_string(damage_dealt) + " " + std::to_string(target_hp));
        if (npc_died) {
            broadcast_to_room(room_id, "", "EVT ROOM COMBAT_DEATH " + npc_id);
            log_info("npc_defeated", {{"player", session->player_id}, {"npc", npc_id}});
            on_npc_defeated(session->player_id, npc_id);
        }
    }
}

void cmd_inventory(const std::shared_ptr<Session>& session) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    auto& world = World::instance();
    std::ostringstream oss;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        const auto& player = world.players.at(session->player_id);
        oss << "OK [";
        for (size_t i = 0; i < player.inventory.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << player.inventory[i] << "\"";
        }
        oss << "]";
    }
    send_line(*session, oss.str());
}
