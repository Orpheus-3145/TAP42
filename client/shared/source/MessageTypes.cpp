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
	assert(it != commands.end() and "command doesn't exist");

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
	assert(it != events.end() and "event doesn't exist");

	return static_cast<EventType>(std::distance(events.begin(), it));
}

bool eventExists(std::string const& event) noexcept
{
	return std::find(events.begin(), events.end(), event) != events.end();
}


std::string toString(ErrorType error)
{
	assert(static_cast<size_t>(error) < errors.size() and "error not mapped");

	return errors[static_cast<size_t>(error)];
}

ErrorType getError(std::string const& error) noexcept
{
	auto it = std::find(errors.begin(), errors.end(), error);
	assert(it != errors.end() and "error doesn't exist");

	return static_cast<ErrorType>(std::distance(errors.begin(), it));
}

uint32_t getErrorCode(ErrorType error)
{
	switch (error)
	{
		case ErrorType::NOT_CONNECTED:			return 102U;
		case ErrorType::BAD_MESSAGE:			return 200U;
		case ErrorType::NAME_IN_USE:			return 201U;
		case ErrorType::NO_EXIT:				return 301U;
		case ErrorType::NOT_IN_GROUP:			return 401U;
		case ErrorType::ALREADY_IN_GROUP:		return 402U;
		case ErrorType::ITEM_NOT_FOUND:
		case ErrorType::ITEM_NOT_IN_INVENTORY:
		case ErrorType::NPC_NOT_FOUND:
		case ErrorType::QUEST_NOT_FOUND:
		case ErrorType::CHARACTER_NOT_FOUND:	return 404U;
		case ErrorType::NPC_NOT_HOSTILE:		return 405U;
		case ErrorType::NO_QUEST_AVAILABLE:		return 406U;
		case ErrorType::DEAD:					return 409U;
		case ErrorType::INTERNAL:				return 500U;
		case ErrorType::CONNECTION_FAILED:		return 900U;
		case ErrorType::SEND_FAILED:			return 901U;
		default:								assert(false and "error not mapped");
	}
}

bool errorExists(std::string const& error) noexcept
{
	return std::find(errors.begin(), errors.end(), error) != errors.end();
}
