// Structured logging to stdout - every line is one JSON object, so it's
// grep-able and easy to feed into whatever log pipeline you point at it.
#pragma once

#include <string>
#include <utility>
#include <vector>

using LogFields = std::vector<std::pair<std::string, std::string>>;

// Structured JSON-line logging to stdout: {"ts":...,"level":...,"event":...,<fields>}
void log_info(const std::string& event, const LogFields& fields = {});
void log_warn(const std::string& event, const LogFields& fields = {});
void log_error(const std::string& event, const LogFields& fields = {});
