#include "commands/dispatcher.hpp"

#include <sstream>
#include <vector>

#include "commands/combat_commands.hpp"
#include "commands/common.hpp"
#include "commands/group_commands.hpp"
#include "commands/item_commands.hpp"
#include "commands/npc_commands.hpp"
#include "commands/quest_commands.hpp"
#include "logging/logger.hpp"
#include "network/registry.hpp"
#include "world/world.hpp"

namespace {

void cmd_connect(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (!session->player_id.empty()) {
        send_line(*session, "ERR ERR_ALREADY_CONNECTED already connected as " + session->player_id);
        return;
    }
    if (args.empty()) {
        send_line(*session, "ERR ERR_BAD_ARGS CONNECT requires a name");
        return;
    }
    std::string name = args[0];

    auto& world = World::instance();
    std::string response;
    bool connected_ok = false;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        if (world.players.count(name)) {
            response = "ERR ERR_NAME_TAKEN name already in use";
        } else {
            PlayerState p;
            p.player_id = name;
            p.current_room = "loc.start";
            init_player_quests_locked(p);
            world.players[name] = p;
            session->player_id = name;
            session->current_room = "loc.start";
            response = "OK connected";
            connected_ok = true;
        }
    }
    if (connected_ok) {
        SessionRegistry::instance().add(name, session);
        log_info("player_connected", {{"player", name}, {"ip", session->peer_ip}});
    }
    send_line(*session, response);
}

void cmd_look(const std::shared_ptr<Session>& session) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    auto& world = World::instance();
    std::string response;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto it = world.rooms.find(session->current_room);
        if (it == world.rooms.end()) {
            response = "ERR ERR_INTERNAL room not found";
        } else {
            response = "OK " + serialize_room_locked(it->second);
        }
    }
    send_line(*session, response);
}

void cmd_move(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    if (args.empty()) {
        send_line(*session, "ERR ERR_BAD_ARGS MOVE requires a direction");
        return;
    }
    std::string direction = args[0];
    auto& world = World::instance();
    std::string response, old_room, new_room;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto& player = world.players.at(session->player_id);
        auto& room = world.rooms.at(player.current_room);
        auto exit_it = room.exits.find(direction);
        if (exit_it == room.exits.end()) {
            response = "ERR ERR_NO_EXIT no exit " + direction;
        } else {
            old_room = player.current_room;
            new_room = exit_it->second;
            player.current_room = new_room;
            session->current_room = new_room;
            response = "OK room=" + new_room;
        }
    }
    send_line(*session, response);
    if (!new_room.empty()) {
        broadcast_to_room(old_room, session->player_id, "EVT ROOM PRESENCE LEAVE " + session->player_id);
        broadcast_to_room(new_room, session->player_id, "EVT ROOM PRESENCE ENTER " + session->player_id);
        log_info("player_moved",
                 {{"player", session->player_id}, {"from", old_room}, {"to", new_room}});
    }
}

void cmd_who(const std::shared_ptr<Session>& session) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    auto& world = World::instance();
    std::ostringstream oss;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        oss << "OK {\"room\":[";
        bool first = true;
        for (auto& [pid, player] : world.players) {
            if (player.current_room != session->current_room) continue;
            if (!first) oss << ",";
            oss << "\"" << pid << "\"";
            first = false;
        }
        oss << "],\"server\":" << world.players.size() << "}";
    }
    send_line(*session, oss.str());
}

void cmd_chat(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    if (args.size() < 2) {
        send_line(*session, "ERR ERR_BAD_ARGS CHAT requires scope and text");
        return;
    }
    std::string scope = args[0];
    if (scope != "GLOBAL" && scope != "ROOM" && scope != "GROUP") {
        send_line(*session, "ERR ERR_BAD_ARGS unknown chat scope " + scope);
        return;
    }
    std::string text = join_from(args, 1);
    std::string evt = "EVT " + scope + " CHAT " + session->player_id + " " + text;

    if (scope == "GROUP") {
        auto& world = World::instance();
        std::string group_id;
        {
            std::lock_guard<std::mutex> lock(world.mutex);
            group_id = world.player_group.count(session->player_id) ? world.player_group.at(session->player_id) : "";
        }
        if (group_id.empty()) {
            send_line(*session, "ERR ERR_NOT_IN_GROUP not in a group");
            return;
        }
        send_line(*session, "OK");
        broadcast_to_group(group_id, "", evt);
        log_info("chat", {{"player", session->player_id}, {"scope", scope}});
        return;
    }

    send_line(*session, "OK");
    if (scope == "GLOBAL") {
        for (auto& s : SessionRegistry::instance().all()) send_line(*s, evt);
    } else {
        broadcast_to_room(session->current_room, "", evt);
    }
    log_info("chat", {{"player", session->player_id}, {"scope", scope}});
}

void cmd_quit(const std::shared_ptr<Session>& session) {
    send_line(*session, "OK bye");
    session->connected = false;
}

} // namespace

void handle_command(std::shared_ptr<Session> session, const std::string& line) {
    auto tokens = split_ws(line);
    if (tokens.empty()) {
        send_line(*session, "ERR ERR_BAD_ARGS empty command");
        return;
    }
    std::string cmd = tokens[0];
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    log_info("command_received",
             {{"player", session->player_id.empty() ? "-" : session->player_id}, {"command", cmd}});

    if (session->rate_tracker.record_and_check_flood()) {
        log_warn("command_flood_detected",
                 {{"player", session->player_id.empty() ? "-" : session->player_id}, {"ip", session->peer_ip}});
    }

    if (cmd == "CONNECT") {
        cmd_connect(session, args);
    } else if (cmd == "LOOK") {
        cmd_look(session);
    } else if (cmd == "MOVE") {
        cmd_move(session, args);
    } else if (cmd == "WHO") {
        cmd_who(session);
    } else if (cmd == "CHAT") {
        cmd_chat(session, args);
    } else if (cmd == "TAKE") {
        cmd_take(session, args);
    } else if (cmd == "DROP") {
        cmd_drop(session, args);
    } else if (cmd == "INVENTORY") {
        cmd_inventory(session);
    } else if (cmd == "TALK") {
        cmd_talk(session, args);
    } else if (cmd == "ATTACK") {
        cmd_attack(session, args);
    } else if (cmd == "STATUS") {
        cmd_status(session);
    } else if (cmd == "QUEST") {
        cmd_quest(session, args);
    } else if (cmd == "QUESTS") {
        cmd_quests(session);
    } else if (cmd == "GROUP") {
        cmd_group(session, args);
    } else if (cmd == "QUIT") {
        cmd_quit(session);
    } else {
        send_line(*session, "ERR ERR_UNKNOWN_CMD unknown command " + cmd);
    }
}

void handle_disconnect(std::shared_ptr<Session> session) {
    if (session->player_id.empty()) return;

    auto& world = World::instance();
    std::string room_id, group_id;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto it = world.players.find(session->player_id);
        if (it != world.players.end()) {
            room_id = it->second.current_room;
            world.players.erase(it);
        }
        group_id = world.player_group.count(session->player_id) ? world.player_group.at(session->player_id) : "";
        leave_group_locked(session->player_id);
    }
    SessionRegistry::instance().remove(session->player_id);
    log_info("player_disconnected", {{"player", session->player_id}});

    if (!room_id.empty()) {
        broadcast_to_room(room_id, session->player_id, "EVT ROOM PRESENCE LEAVE " + session->player_id);
    }
    if (!group_id.empty()) {
        broadcast_to_group(group_id, session->player_id, "EVT GROUP LEAVE " + session->player_id);
    }
}
