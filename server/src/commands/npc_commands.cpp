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
            size_t& progress = player.npc_dialogue_progress[npc_id]; // 0 on first TALK to this npc
            std::string line = npc.dialogue.empty() ? "" : npc.dialogue[progress % npc.dialogue.size()];
            if (!npc.dialogue.empty()) progress = (progress + 1) % npc.dialogue.size();
            std::ostringstream oss;
            oss << "OK {\"npc\":\"" << npc_id << "\",\"dialogue\":\"" << json_escape(line) << "\"}";
            response = oss.str();
        }
    }
    send_line(*session, response);
}
