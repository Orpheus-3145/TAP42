#include "commands/group_commands.hpp"

#include <algorithm>

#include "commands/common.hpp"
#include "logging/logger.hpp"
#include "network/registry.hpp"

namespace {

void cmd_group_invite(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        send_line(*session, "ERR ERR_BAD_ARGS GROUP INVITE requires a player name");
        return;
    }
    std::string target = args[1];
    if (target == session->player_id) {
        send_line(*session, "ERR ERR_BAD_ARGS cannot invite yourself");
        return;
    }

    auto& world = World::instance();
    std::string response, group_id;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        if (!world.players.count(target)) {
            response = "ERR ERR_TARGET_NOT_FOUND " + target;
        } else {
            group_id = world.player_group[session->player_id];
            if (group_id.empty()) {
                group_id = session->player_id; // the group takes its founder's id
                world.player_group[session->player_id] = group_id;
                world.groups[group_id] = {session->player_id};
            }
            world.pending_invites[target] = session->player_id;
            response = "OK invited=" + target;
        }
    }
    send_line(*session, response);
    if (!group_id.empty()) {
        auto target_session = SessionRegistry::instance().get(target);
        if (target_session) send_line(*target_session, "EVT GROUP INVITE " + session->player_id);
        log_info("group_invite", {{"inviter", session->player_id}, {"invited", target}, {"group", group_id}});
    }
}

void cmd_group_accept(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        send_line(*session, "ERR ERR_BAD_ARGS GROUP ACCEPT requires the inviter's name");
        return;
    }
    std::string inviter = args[1];

    auto& world = World::instance();
    std::string response, group_id;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto it = world.pending_invites.find(session->player_id);
        if (it == world.pending_invites.end() || it->second != inviter) {
            response = "ERR ERR_BAD_ARGS no pending invite from " + inviter;
        } else {
            group_id = world.player_group[inviter];
            if (group_id.empty()) {
                response = "ERR ERR_INTERNAL inviter is no longer in a group";
            } else {
                // Must leave any previous group BEFORE joining the new one:
                // otherwise they'd stay registered as a member of both,
                // receiving CHAT GROUP from the group they just left too.
                leave_group_locked(session->player_id);
                world.groups[group_id].push_back(session->player_id);
                world.player_group[session->player_id] = group_id;
                world.pending_invites.erase(it);
                response = "OK joined=" + group_id;
            }
        }
    }
    send_line(*session, response);
    if (!group_id.empty()) {
        broadcast_to_group(group_id, session->player_id, "EVT GROUP JOIN " + session->player_id);
        log_info("group_join", {{"player", session->player_id}, {"group", group_id}});
    }
}

void cmd_group_leave(const std::shared_ptr<Session>& session) {
    auto& world = World::instance();
    std::string group_id;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        group_id = world.player_group[session->player_id];
        leave_group_locked(session->player_id);
    }
    send_line(*session, "OK left");
    if (!group_id.empty()) {
        broadcast_to_group(group_id, "", "EVT GROUP LEAVE " + session->player_id);
        log_info("group_leave", {{"player", session->player_id}, {"group", group_id}});
    }
}

} // namespace

void leave_group_locked(const std::string& player_id) {
    auto& world = World::instance();
    auto it = world.player_group.find(player_id);
    if (it == world.player_group.end() || it->second.empty()) return;
    std::string group_id = it->second;
    auto& members = world.groups[group_id];
    members.erase(std::remove(members.begin(), members.end(), player_id), members.end());
    if (members.empty()) world.groups.erase(group_id);
    world.player_group.erase(it);
}

void cmd_group(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    if (args.empty()) {
        send_line(*session, "ERR ERR_BAD_ARGS GROUP requires a subcommand");
        return;
    }
    if (args[0] == "INVITE") {
        cmd_group_invite(session, args);
    } else if (args[0] == "ACCEPT") {
        cmd_group_accept(session, args);
    } else if (args[0] == "LEAVE") {
        cmd_group_leave(session);
    } else {
        send_line(*session, "ERR ERR_BAD_ARGS unknown GROUP subcommand " + args[0]);
    }
}
