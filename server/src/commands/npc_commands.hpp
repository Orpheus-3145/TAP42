// TALK - one command, one NPC, one line of dialogue at a time.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "network/session.hpp"

void cmd_talk(const std::shared_ptr<Session>& session, const std::vector<std::string>& args);
