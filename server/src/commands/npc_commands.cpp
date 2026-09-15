#include "commands/npc_commands.hpp"

#include <sstream>

#include "commands/common.hpp"
#include "logging/logger.hpp"

// Design choice: an NPC's dialogue is cyclic (each TALK shows the next
// line, wrapping back to the first after the last), not random or a
// multiple-choice menu — simpler to implement and predictable to test.
// Progress through it is tracked per PLAYER (PlayerState::npc_dialogue_progress),
// not on the NPC itself: two players talking to the same bartender each
// hear line 1 first, independently of what the other has already heard.
void cmd_talk(const std::shared_ptr<Session>& session, const std::vector<std::string>& args) {
    if (session->player_id.empty()) {
        send_line(*session, "ERR ERR_NOT_CONNECTED CONNECT first");
        return;
    }
    if (args.empty()) {
        send_line(*session, "ERR ERR_BAD_ARGS TALK requires an npc");
        return;
    }
    std::string ref = join_from(args, 0);

    auto& world = World::instance();
    std::string response;
    {
        std::lock_guard<std::mutex> lock(world.mutex);
        auto& player = world.players.at(session->player_id);
        const auto& room = world.rooms.at(player.current_room);
        std::string npc_id = resolve_npc_ref_locked(room.npc_ids, ref);
        if (npc_id.empty()) {
            response = "ERR ERR_NPC_NOT_FOUND " + ref;
        } else {
            const auto& npc = world.npcs.at(npc_id);

            // bonus_dialogue only joins the cycle once every quest listed in
            // unlock_quest_ids shows "completed" for THIS player.
            bool bonus_unlocked = !npc.unlock_quest_ids.empty();
            for (const auto& qid : npc.unlock_quest_ids) {
                auto status_it = player.quest_status.find(qid);
                if (status_it == player.quest_status.end() || status_it->second != "completed") {
                    bonus_unlocked = false;
                    break;
                }
            }

            size_t base_size = npc.dialogue.size();
            size_t total_size = base_size + (bonus_unlocked ? npc.bonus_dialogue.size() : 0);
            size_t& progress = player.npc_dialogue_progress[npc_id]; // 0 on first TALK to this npc
            std::string line;
            if (total_size > 0) {
                size_t idx = progress % total_size;
                line = idx < base_size ? npc.dialogue[idx] : npc.bonus_dialogue[idx - base_size];
                progress = (progress + 1) % total_size;
            }
            std::ostringstream oss;
            oss << "OK {\"npc\":\"" << npc_id << "\",\"dialogue\":\"" << json_escape(line) << "\"}";
            response = oss.str();
        }
    }
    send_line(*session, response);
}
