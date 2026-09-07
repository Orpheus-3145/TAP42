// No ACCEPT command - every quest is just "in_progress" from the moment
// you connect. Simpler, and the RFC never asked for an accept step anyway.
#include "commands/quest_commands.hpp"

#include <sstream>

#include "commands/common.hpp"
#include "logging/logger.hpp"
#include "network/registry.hpp"

namespace {

std::string quest_type_str(QuestType t) { return t == QuestType::Fetch ? "fetch" : "defeat"; }

void complete_quest_and_notify(const std::string& player_id, const std::string& quest_id,
                                const std::string& trigger, const std::string& trigger_ref) {
    auto session = SessionRegistry::instance().get(player_id);
    if (session) send_line(*session, "EVT QUEST COMPLETE " + player_id + " " + quest_id);
    log_info("quest_completed",
             {{"player", player_id}, {"quest", quest_id}, {"trigger", trigger}, {"ref", trigger_ref}});
}

} // namespace

void init_player_quests_locked(PlayerState& player) {
    auto& world = World::instance();
    for (auto& [qid, quest] : world.quests) {
        if (!player.quest_status.count(qid)) player.quest_status[qid] = "in_progress";
    }
}

void on_item_taken(const std::string& player_id, const std::string& item_id) {
    auto& world = World::instance();
    std::vector<std::string> completed;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto p_it = world.players.find(player_id);
        if (p_it == world.players.end()) return;
        for (auto& [qid, quest] : world.quests) {
            if (quest.type != QuestType::Fetch || quest.target_id != item_id) continue;
            auto status_it = p_it->second.quest_status.find(qid);
            if (status_it == p_it->second.quest_status.end() || status_it->second != "in_progress") continue;
            status_it->second = "completed";
            if (!quest.reward_item_id.empty()) p_it->second.inventory.push_back(quest.reward_item_id);
            completed.push_back(qid);
        }
    }
    for (const auto& qid : completed) complete_quest_and_notify(player_id, qid, "take", item_id);
}

void on_npc_defeated(const std::string& player_id, const std::string& npc_id) {
    auto& world = World::instance();
    std::vector<std::string> completed;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto p_it = world.players.find(player_id);
        if (p_it == world.players.end()) return;
        for (auto& [qid, quest] : world.quests) {
            if (quest.type != QuestType::Defeat || quest.target_id != npc_id) continue;
            auto status_it = p_it->second.quest_status.find(qid);
            if (status_it == p_it->second.quest_status.end() || status_it->second != "in_progress") continue;
            status_it->second = "completed";
            if (!quest.reward_item_id.empty()) p_it->second.inventory.push_back(quest.reward_item_id);
            completed.push_back(qid);
        }
    }
    for (const auto& qid : completed) complete_quest_and_notify(player_id, qid, "defeat", npc_id);
}

void cmd_quests(const std::shared_ptr<Session>& session) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    auto& world = World::instance();
    std::ostringstream oss;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto& player = world.players.at(session->player_id);
        oss << "OK [";
        bool first = true;
        for (auto& [qid, quest] : world.quests) {
            if (!first) oss << ",";
            std::string status = player.quest_status.count(qid) ? player.quest_status.at(qid) : "not_started";
            oss << "{\"id\":\"" << quest.id << "\",\"name\":\"" << json_escape(quest.name)
                << "\",\"status\":\"" << status << "\"}";
            first = false;
        }
        oss << "]";
    }
    send_line(*session, oss.str());
}

void cmd_quest(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    if (args.empty()) {
        send_line(*session, "ERR ERR_BAD_ARGS QUEST requires an id");
        return;
    }
    auto& world = World::instance();
    std::string response;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto q_it = world.quests.find(args[0]);
        if (q_it == world.quests.end()) {
            response = "ERR ERR_QUEST_NOT_FOUND " + args[0];
        } else {
            auto& player = world.players.at(session->player_id);
            std::string status =
                player.quest_status.count(q_it->first) ? player.quest_status.at(q_it->first) : "not_started";
            std::ostringstream oss;
            oss << "OK {\"id\":\"" << q_it->second.id << "\",\"name\":\"" << json_escape(q_it->second.name)
                << "\",\"description\":\"" << json_escape(q_it->second.description) << "\",\"type\":\""
                << quest_type_str(q_it->second.type) << "\",\"status\":\"" << status << "\"}";
            response = oss.str();
        }
    }
    send_line(*session, response);
}
