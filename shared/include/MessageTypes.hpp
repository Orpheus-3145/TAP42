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
	CREATE_PLAYER	// custom
};

static constexpr std::array<const char*, 22> commands
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
	"WAIT HANDSHAKE"
	"CREATE PLAYER"
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
	STATS,						// NB not handled server-side
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
	BAD_MESSAGE = 200U,				//	custom, badly formatted message
	NAME_IN_USE = 201U,				//	Requested username already taken
	NO_EXIT = 301U,					//	Invalid movement direction
	NOT_IN_GROUP = 401U,			//	Group operation requires group membership
	ALREADY_IN_GROUP = 402U,		//	Player already belongs to a group
	NOT_FOUND = 404U,				//	Requested item not available in room
	// ITEM_NOT_FOUND = 404U,		//	Requested item not available in room
	// ITEM_NOT_IN_INVENTORY = 404U,//	Requested item not in player inventory
	// NPC_NOT_FOUND = 404U,		//	Requested NPC not present in room
	NPC_NOT_HOSTILE = 405U,			//	NPC cannot be attacked (not an enemy)
	NO_QUEST_AVAILABLE = 406U,		//	NPC has no quests or quest already completed
	CONNECTION_FAILED = 900U,		//	Connection establishment failed
	SEND_FAILED = 901U				//	Message transmission failed
};
std::string	toString(ErrorType e);
ErrorType	getError(uint32_t code);		// NB temporary and unsafe
