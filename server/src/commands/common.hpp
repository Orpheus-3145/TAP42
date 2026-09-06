// Grab-bag of helpers every command handler ends up needing: tokenizing
// input, broadcasting to a room or group, resolving an item/NPC by name.
#pragma once

#include <string>
#include <vector>

#include "world/world.hpp"

std::vector<std::string> split_ws(const std::string& s);
std::string join_from(const std::vector<std::string>& tokens, size_t start);
std::string to_lower(const std::string& s);
std::string json_escape(const std::string& s);

// Sends `message` to every player in room `room_id`, except `except_player`
// (pass "" to exclude no one).
// Precondition: the caller must NOT already hold world.mutex (this takes it).
void broadcast_to_room(const std::string& room_id, const std::string& except_player,
                        const std::string& message);

// Same as above but for the members of group `group_id`.
void broadcast_to_group(const std::string& group_id, const std::string& except_player,
                         const std::string& message);

// Precondition: the caller must already hold world.mutex.
std::string serialize_room_locked(const Room& room);

// Resolves a user-supplied reference (exact id or case-insensitive display
// name) against a list of candidate ids (e.g. items/npcs present in a room).
// Returns "" if not found. Precondition: world.mutex already held.
std::string resolve_item_ref_locked(const std::vector<std::string>& candidate_ids, const std::string& ref);
std::string resolve_npc_ref_locked(const std::vector<std::string>& candidate_ids, const std::string& ref);
