// ATTACK and STATUS. One command resolves the whole exchange, no waiting
// on a second message to see what happens next.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "network/session.hpp"

void cmd_attack(const std::shared_ptr<Session>& session, const std::vector<std::string>& args);
void cmd_status(const std::shared_ptr<Session>& session);
