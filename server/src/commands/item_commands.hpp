#pragma once

#include <memory>
#include <string>
#include <vector>

#include "network/session.hpp"

void cmd_take(const std::shared_ptr<Session>& session, const std::vector<std::string>& args);
void cmd_drop(const std::shared_ptr<Session>& session, const std::vector<std::string>& args);
void cmd_inventory(const std::shared_ptr<Session>& session);
