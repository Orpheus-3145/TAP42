// Character persistence: one JSON file per character, named after the
// character itself. Saving one player's progress only ever touches that
// player's own file, so two players updating at the same time never
// contend over a shared file the way a single combined character
// database would.
#pragma once

#include <string>

#include "world/world.hpp"

namespace character_store {

// Points future load/exists/save calls at `directory`, creating it if
// missing. Call once at startup, before any client can CONNECT or
// CREATE_CHARACTER.
void init(const std::string& directory);

// A name has to be safe to use as a filename on its own (no path
// separators, no "..") since it is used verbatim as <directory>/<name>.json.
bool is_valid_name(const std::string& name);

// True if a character file already exists for this name.
bool exists(const std::string& name);

// Loads a character's saved state from disk into `out`. Returns false if
// the file is missing, unreadable, or fails to parse.
bool load(const std::string& name, PlayerState& out);

// Serializes and overwrites this character's file. Safe to call without
// holding world.mutex — in fact it must be called that way, since this
// does real disk I/O and holding the world lock across it would stall
// every other player's command for as long as the write takes.
void save(const PlayerState& state);

} // namespace character_store
