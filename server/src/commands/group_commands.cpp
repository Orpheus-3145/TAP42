// leave_group_locked is shared with disconnect handling, and ACCEPT calls it
// too - see the comment inline for the double-membership bug that fixed.
#include "commands/group_commands.hpp"

#include <algorithm>

#include "commands/common.hpp"
#include "logging/logger.hpp"
#include "network/registry.hpp"

namespace {

// Explicit creation, for a client that wants to found a group without also
// naming someone to invite in the same breath. GROUP INVITE still auto-creates
// on first use too (kept for backward compatibility) - CREATE is just a second
// way to reach the same "I now have an empty group" state.
// An optional name (GROUP CREATE <name>) picks the group id; without one it
// falls back to the founder's own id, same as before.
void cmd_group_create(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    auto& world = World::instance();
    std::string response, group_id;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        if (!world.player_group[session->player_id].empty()) {
            response = "ERR 402 ALREADY_IN_GROUP";
        } else {
            std::string requested_id = args.size() > 1 ? args[1] : session->player_id;
            if (world.groups.count(requested_id) > 0) {
                response = "ERR 201 NAME_IN_USE group name already taken";
            } else {
                group_id = requested_id;
                world.player_group[session->player_id] = group_id;
                world.groups[group_id] = {session->player_id};
                response = "OK {\"created\":\"" + group_id + "\"}";
            }
        }
    }
    send_line(*session, response);
    if (!group_id.empty()) {
        log_info("group_created", {{"player", session->player_id}, {"group", group_id}});
    }
}

void cmd_group_invite(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        send_line(*session, "ERR 200 BAD_MESSAGE GROUP INVITE requires a player name");
        return;
    }
    std::string target = args[1];
    if (target == session->player_id) {
        send_line(*session, "ERR 200 BAD_MESSAGE cannot invite yourself");
        return;
    }

    auto& world = World::instance();
    std::string response, group_id;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        if (!world.players.count(target)) {
            response = "ERR 404 PLAYER_NOT_FOUND " + target;
        } else {
            group_id = world.player_group[session->player_id];
            if (group_id.empty()) {
                response = "ERR 401 NOT_IN_GROUP";
            } else {
                world.pending_invites.emplace(target, group_id);
                response = "OK {\"invited\":\"" + target + "\"}";
            }
        }
    }
    send_line(*session, response);
    if (!group_id.empty()) {
        auto target_session = SessionRegistry::instance().get(target);
        if (target_session) {
            send_line(*target_session, "EVT GROUP INVITE " +
                          ("{\"inviter\":\"" + session->player_id + "\"," +
                           "\"invited\":\"" + target + "\"," +
                           "\"group\":\"" + group_id + "\"}"));
        }
        log_info("group_invite", {{"inviter", session->player_id}, {"invited", target}, {"group", group_id}});
    }
}

void cmd_group_accept(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        send_line(*session, "ERR 200 BAD_MESSAGE GROUP ACCEPT requires the group's name");
        return;
    }
    std::string groupToJoin = args[1];

    auto& world = World::instance();
    std::string response, group_id;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto rangeInvites = world.pending_invites.equal_range(session->player_id);
        if (rangeInvites.first == world.pending_invites.end()) {
            response = "ERR 200 BAD_MESSAGE no pending invite for " + session->player_id;
        } else {
            // there might be multiple invites to the same person
            // to different groups, find the one; specific to the group to join
            for (; rangeInvites.first != rangeInvites.second; rangeInvites.first++)
            {
                if (rangeInvites.first->second != groupToJoin)
                    continue;

                // Must leave any previous group BEFORE joining the new one:
                // otherwise they'd stay registered as a member of both,
                // receiving CHAT GROUP from the group they just left too.
                group_id = rangeInvites.first->second;
                leave_group_locked(session->player_id);
                world.groups[group_id].push_back(session->player_id);
                world.player_group[session->player_id] = group_id;
                world.pending_invites.erase(rangeInvites.first);
                response = "OK {\"joined\":\"" + group_id + "\"}";
                break;
            }
            if (rangeInvites.first == rangeInvites.second)
                response = "ERR 500 INTERNAL no invite from group " + groupToJoin;
        }
    }
    send_line(*session, response);
    if (!group_id.empty()) {
        broadcast_to_group(group_id, session->player_id, "EVT GROUP JOIN " +
                               ("{\"player\":\"" + session->player_id + "\"," +
                                "\"group\":\"" + group_id + "\"}"));
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
    send_line(*session, "OK {\"left\":\"" + group_id + "\"}");
    if (!group_id.empty()) {
        broadcast_to_group(group_id, "", "EVT GROUP LEAVE " +
                               ("{\"player\":\"" + session->player_id + "\"," +
                                "\"group\":\"" + group_id + "\"}"));
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
        send_line(*session, "ERR 102 NOT_CONNECTED CONNECT first");
        return;
    }
    if (args.empty()) {
        send_line(*session, "ERR 200 BAD_MESSAGE GROUP requires a subcommand");
        return;
    }
    if (args[0] == "CREATE") {
        cmd_group_create(session, args);
    } else if (args[0] == "INVITE") {
        cmd_group_invite(session, args);
    } else if (args[0] == "ACCEPT" || args[0] == "JOIN") {
        // "JOIN" is a compatibility alias: the CLI client currently sends
        // "GROUP JOIN <inviter>" rather than "GROUP ACCEPT <inviter>".
        cmd_group_accept(session, args);
    } else if (args[0] == "LEAVE") {
        cmd_group_leave(session);
    } else {
        send_line(*session, "ERR 200 BAD_MESSAGE unknown GROUP subcommand " + args[0]);
    }
}
