#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

#include "JsonParser.hpp"


enum class MessageType : uint8_t
{
	COMMAND = 0U,
	RESPONSE,
	ERROR,
	EVENT
};

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
	USE // custom
};

static constexpr std::array<const char*, 21> commands
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
	"USE"
};

std::string		toString(CommandType cmd);
CommandType		getCommand(std::string const& cmdStr);
inline bool		commandExists(std::string const& cmd) { return std::find(commands.begin(), commands.end(), cmd) != commands.end(); }

enum class ErrorType : uint16_t
{
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
std::string			toString(ErrorType e);

enum class EventType : uint16_t
{
	PRESENCE_ENTER = 0U,	// EVT ROOM PRESENCE_ENTER	Player entered current room
	PRESENCE_LEAVE,			// EVT ROOM PRESENCE_LEAVE	Player left current room
	ROOM_CHAT,				// EVT ROOM CHAT	Room-scoped chat message
	GROUP_CHAT,				// EVT GROUP CHAT	Group-scoped chat message
	GLOBAL_CHAT,			// EVT GLOBAL CHAT	Server-wide chat message
	GROUP_JOIN,				// EVT GROUP JOIN	Player joined group
	GROUP_LEAVE,			// EVT GROUP LEAVE	Player left group
	GROUP_INVITE,			// EVT GROUP INVITE	Group invitation received
	STATS,					// EVT STATS players=	Updated player count
	// custom events
	COMBAT,
	COMBAT_DEATH_NPC,
	COMBAT_DEATH_PLAYER,
	ITEM_USE,
	QUEST_COMPLETE
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

std::string		toString(EventType e);
EventType		getEvent(std::string const& visibility, std::string const& name);
inline bool		eventExists(std::string const& event) { return std::find(events.begin(), events.end(), event) != events.end(); }

union MessageSubType
{
	ErrorType	errType;
	EventType	eventType;
	CommandType commandType;
};

static constexpr const char*	S_OK = "OK";
static constexpr const char*	S_ERR = "ERR";
static constexpr const char*	S_EVT = "EVT";

class Message
{
	public:
		explicit Message(std::string const& message) :
			raw{message} { this->parse(message); }

		Message(Message const& other) = default;
		Message& operator=(Message const& other) = default;
		Message(Message&& other) = default;
		Message& operator=(Message&& other) = default;

		~Message(void) noexcept = default;

		bool isCommand(void) const noexcept { return this->type == MessageType::COMMAND; }
		bool isResponse(void) const noexcept { return this->type == MessageType::RESPONSE; }
		bool isError(void) const noexcept { return this->type == MessageType::ERROR; }
		bool isEvent(void) const noexcept { return this->type == MessageType::EVENT; }

		std::string	formatAsCommand(void) const;
		std::string	formatAsResponse(CommandType command) const;
		std::string	formatAsError(void) const;
		std::string	formatAsEvent(void) const;

	private:
		void parse(std::string const& message);
		void parseResponse(std::string const& payload);
		void parseError(std::string const& payload);
		void parseEvent(std::string const& payload);
		void parseCommand(std::string const& payload);

		void storeJson(std::string const& jsonStr);
		void storeKeyValueJson(std::string const& keyValue);
		void storeStringJson(std::string const& str);

		std::string		raw;
		MessageType		type;
		MessageSubType	subType;
		JsonValue		data;
};