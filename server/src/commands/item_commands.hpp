// TAKE, DROP, INVENTORY - moving item instances between a room and a
// player's inventory. No duplication, ever.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "network/session.hpp"

void cmd_take(const std::shared_ptr<Session>& session, const std::vector<std::string>& args);
void cmd_drop(const std::shared_ptr<Session>& session, const std::vector<std::string>& args);
void cmd_inventory(const std::shared_ptr<Session>& session);

// USE <item> [target] — a server extension, not part of the base RFC command
// set (documented in PROTOCOL.md). Heal-type items need no target; damage-type
// items (thrown at an NPC) do.
void cmd_use(const std::shared_ptr<Session>& session, const std::vector<std::string>& args);
