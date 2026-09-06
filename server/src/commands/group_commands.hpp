#pragma once

#include <memory>
#include <string>
#include <vector>

#include "network/session.hpp"

void cmd_group(const std::shared_ptr<Session>& session, const std::vector<std::string>& args);

// Removes the player from their group, if they have one. Also used on disconnect.
// Precondition: world.mutex already held by the caller.
void leave_group_locked(const std::string& player_id);
