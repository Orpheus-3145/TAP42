#pragma once

#include <string>

// Loads the world data from a JSON file into World::instance(), validating
// that exits, placed items/npcs, and quest references all point to existing
// entities. Returns false (with log_error) on the first read, parse, or
// validation error.
bool load_world(const std::string& path);
