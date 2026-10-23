#pragma once

#include <string>
#include <cstdint>
#include <array>
#include <algorithm>


enum class CommandType : uint16_t
{
	CONNECT = 0U,
	LOOK,
	MOVE,
	WHO,
	TAKE,
	DROP,
	INVENTORY,
	TALK,
	ATTACK,
	STATUS,
	QUEST,
	QUESTS,
	QUIT,
	GROUP_CREATE,
	GROUP_INVITE,
	GROUP_JOIN,
	GROUP_LEAVE,
	GROUP_CHAT,
	ROOM_CHAT,
	GLOBAL_CHAT,
	USE,			// custom
	WAIT_HANDSHAKE,	// custom
	CREATE_PLAYER,	// custom
	PROFILE			// custom
};

static constexpr std::array<const char*, 24> commands
{
	"CONNECT",
	"LOOK",
	"MOVE",
	"WHO",
	"TAKE",
	"DROP",
	"INVENTORY",
	"TALK",
	"ATTACK",
	"STATUS",
	"QUEST",
	"QUESTS",
	"QUIT",
	"GROUP CREATE",
	"GROUP INVITE",
	"GROUP JOIN",
	"GROUP LEAVE",
	"CHAT GROUP",
	"CHAT ROOM",
	"CHAT GLOBAL",
	"USE",
	"WAIT HANDSHAKE",
	"CREATE PLAYER",
	"PROFILE"
};

std::string	toString(CommandType cmd);
CommandType	getCommand(std::string const& cmdStr) noexcept;
bool		commandExists(std::string const& cmd) noexcept;
bool		commandHasArgs(CommandType type) noexcept;


enum class EventType : uint16_t
{
	PRESENCE_ENTER = 0U,
	PRESENCE_LEAVE,
	ROOM_CHAT,
	GROUP_CHAT,
	GLOBAL_CHAT,
	GROUP_JOIN,
	GROUP_LEAVE,
	GROUP_INVITE,
	STATS,
	ROOM_COMBAT,				// custom
	ROOM_COMBAT_DEATH_NPC,		// custom
	ROOM_COMBAT_DEATH_PLAYER,	// custom
	ROOM_ITEM_USE,				// custom
	ROOM_QUEST_COMPLETE			// custom
};

static constexpr std::array<const char*, 14> events
{
	"ROOM PRESENCE_ENTER",
	"ROOM PRESENCE_LEAVE",
	"ROOM CHAT",
	"GROUP CHAT",
	"GLOBAL CHAT",
	"GROUP JOIN",
	"GROUP LEAVE",
	"GROUP INVITE",
	"STATS",
	"ROOM COMBAT",
	"ROOM COMBAT_DEATH_NPC",
	"ROOM COMBAT_DEATH_PLAYER",
	"ROOM ITEM_USE",
	"ROOM QUEST_COMPLETE"
};

std::string	toString(EventType e);
EventType	getEvent(std::string const& event) noexcept;
bool		eventExists(std::string const& event) noexcept;


enum class ErrorType : uint16_t
{
	NOT_CONNECTED = 0,		// custom
	BAD_MESSAGE,			// custom, badly formatted message
	NAME_IN_USE,			// Requested username already taken
	NO_EXIT,				// Invalid movement direction
	NOT_IN_GROUP,			// Group operation requires group membership
	ALREADY_IN_GROUP,		// Player already belongs to a group
	ITEM_NOT_FOUND,			// Requested item not available in room
	ITEM_NOT_IN_INVENTORY,	// Requested item not in player inventory
	NPC_NOT_FOUND,			// Requested NPC not present in room
	QUEST_NOT_FOUND,		// custom
	CHARACTER_NOT_FOUND,	// custom
	NPC_NOT_HOSTILE,		// NPC cannot be attacked (not an enemy)		NB not implemented server-side
	NO_QUEST_AVAILABLE,		// NPC has no quests or quest already completed	NB not implemented server-side
	DEAD,					// custom
	INTERNAL,				// custom
	CONNECTION_FAILED,		// Connection establishment failed
	SEND_FAILED				// Message transmission failed
};

static constexpr std::array<const char*,17> errors
{
	"NOT_CONNECTED",
	"BAD_MESSAGE",
	"NAME_IN_USE",
	"NO_EXIT",
	"NOT_IN_GROUP",
	"ALREADY_IN_GROUP",
	"ITEM_NOT_FOUND",
	"ITEM_NOT_IN_INVENTORY",
	"NPC_NOT_FOUND",
	"QUEST_NOT_FOUND",
	"CHARACTER_NOT_FOUND",
	"NPC_NOT_HOSTILE",
	"NO_QUEST_AVAILABLE",
	"DEAD",
	"INTERNAL",
	"CONNECTION_FAILED",
	"SEND_FAILED"
};

std::string	toString(ErrorType e);
ErrorType	getError(std::string const& error) noexcept;
uint32_t	getErrorCode(ErrorType error);
bool		errorExists(std::string const& error) noexcept;
