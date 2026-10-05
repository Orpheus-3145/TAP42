#include "Message.hpp"
#include "Exceptions.hpp"

#include <format>
#include <sstream>
#include <iostream>
#include <cassert>


std::string toString(CommandType cmd)
{
	size_t indexCommand = static_cast<size_t>(cmd);
	if (indexCommand >= commands.size())
		throw AppException(ErrorCode::BAD_MESSAGE, std::format("Unknown command: {}", indexCommand));

	return commands[indexCommand];
}

CommandType getCommand(std::string const& cmd)
{
	auto it = std::find(commands.begin(), commands.end(), cmd);
	if (it == commands.end())
		throw AppException(ErrorCode::BAD_MESSAGE, std::format("Unknown command: {}", cmd));

	return static_cast<CommandType>(std::distance(commands.begin(), it));
}

std::string toString(ErrorType e)
{
    switch (e)
	{
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

std::string toString(EventType event)
{
	size_t indexEvent = static_cast<size_t>(event);
	if (indexEvent >= events.size())
		throw AppException(ErrorCode::BAD_MESSAGE, std::format("Unknown event: {}", indexEvent));

	return events[indexEvent];
}

EventType getEvent(std::string const& event)
{
	auto it = std::find(events.begin(), events.end(), event);
	if (it == events.end())
		throw AppException(ErrorCode::BAD_MESSAGE, std::format("Unknown event: {}", event));

	return static_cast<EventType>(std::distance(events.begin(), it));
}


std::string	Message::formatAsResponse(CommandType command) const
{
	(void) command;
	std::string	formatted = "lorem ipsum";

	return formatted;
}

std::string	Message::formatAsError(void) const
{
	std::string	formatted = "lorem ipsum";

	return formatted;
}

std::string	Message::formatAsEvent(void) const
{
	std::string	formatted = "lorem ipsum";

	return formatted;
}

std::string	Message::formatAsCommand(void) const
{
	std::string	formatted = "lorem ipsum";

	return formatted;
}

void Message::parse(std::string const& message)
{
	std::istringstream	iss(message);
	std::string			msgType, secondPart;

	if (!(iss >> msgType))
		throw AppException(ErrorCode::BAD_MESSAGE, "Empty message: " + message);
	
	if (msgType == S_OK)
	{
		this->type = MessageType::RESPONSE;
		if ((msgType.size() + 1UL) < message.size())
			this->parseResponse(message);
	}
	else if (msgType == S_ERR)
	{
		if ((msgType.size() + 1UL) >= message.size())
			throw AppException(ErrorCode::BAD_MESSAGE, "Invalid error: " + message);

		this->type = MessageType::ERROR;
		this->parseError(message);
	}
	else if (msgType == S_EVT)
	{
		if ((msgType.size() + 1UL) >= message.size())
			throw AppException(ErrorCode::BAD_MESSAGE, "Invalid event: " + message);

		this->type = MessageType::EVENT;
		this->parseEvent(message);
	}
	else
	{
		this->type = MessageType::COMMAND;
		this->parseCommand(message);
	}
}

void Message::parseResponse(std::string const& response)
{
	this->type = MessageType::RESPONSE;

	std::istringstream	iss(response);
	std::string			ok, data;

	if (!(iss >> ok))
		throw AppException(ErrorCode::BAD_MESSAGE, "Empty response: " + response);
	std::getline(iss >> std::ws, data);

	if (data.empty() == true)	// responses to CHAT, INVITE and LEAVE are just 'OK'
		return;

	if (data[0] == '{')								// data is json
		this->storeJson(data);
	else if (data.find('=') != std::string::npos)	// data is in format key=value
		this->storeKeyValueJson(data);
	else			// data is just a simple string (in case of commands CONNECT [OK connected] QUIT [OK bye] TALK [OK <talking>])
		this->storeStringJson(data);
}

void Message::parseError(std::string const& error)
{
	this->type = MessageType::ERROR;

	std::istringstream	iss(error);
	std::string			err, errInfo;
	uint16_t			errCode;

	if (!(iss >> err >> errCode >> errInfo))
		throw AppException(ErrorCode::BAD_MESSAGE, "Incomplete error: " + error);
	this->subType.errType = static_cast<ErrorType>(errCode);
}

void Message::parseEvent(std::string const& event)
{
	this->type = MessageType::EVENT;

	std::istringstream	iss(event);
	std::string			evt, visibility, type, data;

	if (!(iss >> evt >> visibility >> type))
		throw AppException(ErrorCode::BAD_MESSAGE, "Incomplete event: " + event);

	if (visibility == "STATS")	// STATS event doesn't have the type, therefore its data is stored in type
	{
		this->subType.eventType = EventType::STATS;
		data = type;
	}
	else
	{
		this->subType.eventType = getEvent(std::format("{} {}", visibility, type));
		std::getline(iss >> std::ws, data);
		if (data.empty() == true)
			throw AppException(ErrorCode::BAD_MESSAGE, "Missing data for event: " + event);
	}
	if (data.find('=') != std::string::npos)	// data is in format key=value
		this->storeKeyValueJson(data);
	else			// data is just a simple string (in case of commands CONNECT [OK connected] QUIT [OK bye] TALK [OK <talking>])
		this->storeStringJson(data);
}

void Message::parseCommand(std::string const& command)
{
	this->type = MessageType::COMMAND;

	std::istringstream	iss(command);
	std::string	cmd, secondCmd, data;

	if (!(iss >> cmd))
		throw AppException(ErrorCode::BAD_MESSAGE, "Empty message: " + command);

	if ((cmd == "GROUP") or (cmd == "CHAT"))		// there are command with two words
	{
		if (!(iss >> secondCmd))
			throw AppException(ErrorCode::BAD_MESSAGE, "Incomplete command: " + command);
		cmd += " " + secondCmd;
	}

	this->subType.commandType = getCommand(cmd);
	if (commandExists(cmd) == false)
		throw AppException(ErrorCode::BAD_MESSAGE, "Unrecognised command: " + command);

	switch (this->subType.commandType)
	{
		case CommandType::CONNECT:
		case CommandType::MOVE:
		case CommandType::GROUP_CHAT:
		case CommandType::ROOM_CHAT:
		case CommandType::GROUP_CREATE:
		case CommandType::GROUP_JOIN:
		case CommandType::GROUP_INVITE:
		case CommandType::TAKE:
		case CommandType::DROP:
		case CommandType::TALK:
		case CommandType::ATTACK:
		case CommandType::QUEST:
		case CommandType::USE:
			std::getline(iss >> std::ws, data);
			if (data.empty() == true)
				throw AppException(ErrorCode::BAD_MESSAGE, "Missing data for command: " + command);
			this->storeStringJson(data);
			break;

		default:	// commands with no args: LOOK, QUIT, WHO, INVENTORY, STATUS, QUEST, GROUP LEAVE
			break;
	}
}

void Message::storeJson(std::string const& jsonStr)
{
	std::string error;
	if (parse_json(jsonStr, this->data, error) == false)
		throw AppException(ErrorCode::BAD_MESSAGE, std::format("Error in '{}' while parsing json: {}", jsonStr, error));
}

void Message::storeKeyValueJson(std::string const& keyValue)
{
	size_t eqPos = keyValue.find('=');
	if ((eqPos == 0UL) or (eqPos == keyValue.size() - 1UL))
		throw AppException(ErrorCode::BAD_MESSAGE, "Key-value item missing key or value: " + keyValue);

	std::string key, value;
	key = keyValue.substr(0, eqPos);
	value = keyValue.substr(eqPos + 1);

	if (std::isdigit(value[0]) or (value[0] == '-'))		// string: number
		this->storeJson(std::format("{{\"{}\": {}}}", key, value));
	else													// string: string
		this->storeJson(std::format("{{\"{}\": \"{}\"}}", key, value));
}

void Message::storeStringJson(std::string const& str)
{
	this->storeJson(std::format("\"{}\"", str));
}
