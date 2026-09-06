#pragma once

#include <memory>
#include <string>
#include <vector>

#include "network/session.hpp"
#include "world/world.hpp"

void cmd_quest(const std::shared_ptr<Session>& session, const std::vector<std::string>& args);
void cmd_quests(const std::shared_ptr<Session>& session);

// Initializes the quest state of a freshly connected player: every quest in
// the world starts "in_progress" (the protocol has no explicit ACCEPT
// command, so every quest is considered active by default for everyone).
// Precondition: world.mutex already held by the caller.
void init_player_quests_locked(PlayerState& player);

// Quest progression triggers, to be called WITHOUT world.mutex already held
// (they take it themselves). Notify the player via EVT if a quest completes.
void on_item_taken(const std::string& player_id, const std::string& item_id);
void on_npc_defeated(const std::string& player_id, const std::string& npc_id);
