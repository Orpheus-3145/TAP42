// No ACCEPT command - every quest is just "in_progress" from the moment
// you connect. Simpler, and the RFC never asked for an accept step anyway.
#include "commands/quest_commands.hpp"

#include <algorithm>
#include <sstream>

#include "commands/common.hpp"
#include "logging/logger.hpp"
#include "network/registry.hpp"
#include "world/character_store.hpp"

namespace {

std::string quest_type_str(QuestType t) { return t == QuestType::Fetch ? "fetch" : "defeat"; }

// True once every npc listed in a Defeat quest's target_ids is dead.
// Precondition: world.mutex already held.
bool all_defeat_targets_dead_locked(const Quest& quest) {
    auto& world = World::instance();
    for (const auto& npc_id : quest.target_ids) {
        auto it = world.npcs.find(npc_id);
        if (it == world.npcs.end() || it->second.hp > 0) return false;
    }
    return true;
}

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
        if (player.quest_status.count(qid)) continue;
        // A defeat quest whose targets are already all dead (someone cleared
        // it before this player even connected) starts completed instead of
        // lying about being in_progress forever.
        bool already_done = quest.type == QuestType::Defeat && all_defeat_targets_dead_locked(quest);
        player.quest_status[qid] = already_done ? "completed" : "in_progress";
    }
}

void on_item_taken(const std::string& player_id, const std::string& item_id) {
    auto& world = World::instance();
    std::vector<std::string> completed;
    PlayerState taker;
    bool taker_mutated = false;
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
        if (!completed.empty()) {
            taker = p_it->second;
            taker_mutated = true;
        }
    }
    if (taker_mutated) character_store::save(taker);
    for (const auto& qid : completed) complete_quest_and_notify(player_id, qid, "take", item_id);
}

// Design choice: a defeat quest is world state, not personal credit — "kill
// all the rats" is about the sewers being clear, not about who swung the
// killing blow on each one. So when the last listed npc dies, every player
// who still has the quest in_progress gets it marked completed at once, not
// just whoever landed this particular kill. No reward_item_id on a quest
// that can complete for several players simultaneously: handing the same
// item id to two inventories at once would break instance uniqueness.
void on_npc_defeated(const std::string& player_id, const std::string& npc_id) {
    auto& world = World::instance();
    std::vector<std::pair<std::string, std::string>> completions; // (player_id, quest_id)
    std::vector<PlayerState> mutated_players;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        for (auto& [qid, quest] : world.quests) {
            if (quest.type != QuestType::Defeat) continue;
            bool targets_this_npc =
                std::find(quest.target_ids.begin(), quest.target_ids.end(), npc_id) != quest.target_ids.end();
            if (!targets_this_npc || !all_defeat_targets_dead_locked(quest)) continue;

            for (auto& [pid, pstate] : world.players) {
                auto status_it = pstate.quest_status.find(qid);
                if (status_it == pstate.quest_status.end() || status_it->second != "in_progress") continue;
                status_it->second = "completed";
                if (!quest.reward_item_id.empty()) pstate.inventory.push_back(quest.reward_item_id);
                completions.emplace_back(pid, qid);
                mutated_players.push_back(pstate);
            }
        }
    }
    if (!completions.empty()) {
        // player_id is whoever actually landed this kill; everyone in
        // completions benefits from it, so it's logged separately here
        // rather than being one of the notified players itself.
        log_info("quest_world_cleared", {{"triggered_by", player_id}, {"npc", npc_id}});
    }
    // A player can appear here once per completed quest, so this may save
    // the same character's file twice in a row (e.g. clearing the rats and
    // the rat king at once) - harmless, character_store::save is just an
    // overwrite of the same file with slightly newer content each time.
    for (const auto& p : mutated_players) character_store::save(p);
    for (auto& [pid, qid] : completions) complete_quest_and_notify(pid, qid, "defeat", npc_id);
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
