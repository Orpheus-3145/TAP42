// Straightforward: resolve the reference, move the id between the two
// vectors, tell the quest system if a fetch quest just got satisfied.
#include "commands/item_commands.hpp"

#include <algorithm>
#include <sstream>

#include "commands/common.hpp"
#include "commands/quest_commands.hpp"
#include "logging/logger.hpp"

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
        }
    }
    send_line(*session, response);
    if (!taken_id.empty()) {
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
        }
    }
    send_line(*session, response);
    if (!dropped_id.empty()) {
        log_info("item_dropped", {{"player", session->player_id}, {"item", dropped_id}});
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
