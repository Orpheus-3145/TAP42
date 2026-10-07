#include "MessageTypes.hpp"
#include "Exceptions.hpp"

#include <format>
#include <cassert>


std::string toString(CommandType cmd)
{
	assert(static_cast<size_t>(cmd) < commands.size() and "command not mapped");

	return commands[static_cast<size_t>(cmd)];
}

CommandType getCommand(std::string const& cmd) noexcept
{
	auto it = std::find(commands.begin(), commands.end(), cmd);
	assert(it == commands.end() and "command doesn't exist");

	return static_cast<CommandType>(std::distance(commands.begin(), it));
}

bool commandExists(std::string const& cmd) noexcept
{
	return std::find(commands.begin(), commands.end(), cmd) != commands.end();
}

bool commandHasArgs(CommandType type) noexcept
{
	static constexpr std::array<CommandType, 14> commandsWithArgs
	{
		CommandType::CONNECT,
		CommandType::MOVE,
		CommandType::TAKE,
		CommandType::DROP,
		CommandType::TALK,
		CommandType::ATTACK,
		CommandType::QUEST,
		CommandType::GROUP_CREATE,
		CommandType::GROUP_INVITE,
		CommandType::GROUP_JOIN,
		CommandType::GROUP_CHAT,
		CommandType::ROOM_CHAT,
		CommandType::GLOBAL_CHAT,
		CommandType::USE
	};
	return std::find(commandsWithArgs.begin(), commandsWithArgs.end(), type) != commandsWithArgs.end();
}


std::string toString(EventType event)
{
	assert(static_cast<size_t>(event) < events.size() and "event not mapped");

	return events[static_cast<size_t>(event)];
}

EventType getEvent(std::string const& event) noexcept
{
	auto it = std::find(events.begin(), events.end(), event);
	assert(it == events.end() and "event doesn't exist");

	return static_cast<EventType>(std::distance(events.begin(), it));
}

bool eventExists(std::string const& event) noexcept
{
	return std::find(events.begin(), events.end(), event) != events.end();
}


std::string toString(ErrorType e)
{
    switch (e)
	{
		case ErrorType::BAD_MESSAGE:			return "BAD_MESSAGE";
		case ErrorType::NAME_IN_USE:			return "NAME_IN_USE";
		case ErrorType::NO_EXIT:				return "NO_EXIT";
		case ErrorType::NOT_IN_GROUP:			return "NOT_IN_GROUP";
		case ErrorType::ALREADY_IN_GROUP:		return "ALREADY_IN_GROUP";
		case ErrorType::NOT_FOUND:				return "ITEM_NOT_FOUND";
		case ErrorType::NPC_NOT_HOSTILE:		return "NPC_NOT_HOSTILE";
		case ErrorType::NO_QUEST_AVAILABLE:		return "NO_QUEST_AVAILABLE";
		case ErrorType::CONNECTION_FAILED:		return "CONNECTION_FAILED";
		case ErrorType::SEND_FAILED:			return "SEND_FAILED";
		default:								throw AppException(ErrorCode::BAD_MESSAGE, std::format("Unknown enum value: {}", static_cast<uint32_t>(e)));
    }
}

ErrorType getError(uint32_t code)
{
	return static_cast<ErrorType>(code);
}
