#include "Message.hpp"
#include "Exceptions.hpp"
#include "Config.hpp"

#include <format>
#include <sstream>
#include <iostream>
#include <cassert>


Message::Message(std::string const& message) : raw{message}
{
	if (this->raw.back() == Config::COMMAND_TERM)
		this->raw.pop_back();
	this->parse(this->raw);
}

std::string	Message::toStringCommand(void) const noexcept
{
	assert(this->type == MessageType::COMMAND and "wrong type to stringify");

	return this->getRawMessage();
}

std::string	Message::toStringResponse(CommandType command) const
{
	assert(this->type == MessageType::RESPONSE and "wrong type to stringify");

	switch (command)
	{
		case CommandType::LOOK: 			return std::format("Looking around:\n{}", this->data.toString());
		case CommandType::MOVE: 			return std::format("Moved to room {}", this->data["room"].as_string());
		case CommandType::WHO: 				return std::format("{} players in the room: {}", this->data["server"].as_string(), this->data["room"].toString());
		case CommandType::TAKE: 			return std::format("Taken item {}", this->data["taken"].as_string());
		case CommandType::DROP: 			return std::format("Dropped item {}", this->data["dropped"].as_string());
		case CommandType::INVENTORY: 		return std::format("Inventory:\n{}", this->data.toString());	// array data
		case CommandType::TALK: 			return std::format("{} says: '{}", this->data["npc"].as_string(), this->data["dialogue"].as_string());
		case CommandType::ATTACK: 			return std::format("Attacked:\n{}", this->data.toString());
		case CommandType::STATUS: 			return std::format("Current status:\n{}", this->data.toString());
		case CommandType::QUEST: 			return std::format("Info quest: '{}'", this->data.toString());
		case CommandType::QUESTS: 			return std::format("Quest log:\n{}", this->data.toString());	// array data
		case CommandType::GROUP_CREATE: 	return std::format("Group {} created", this->data["created"].as_string());
		case CommandType::GROUP_INVITE: 	return std::format("Invited {} to join group", this->data["invited"].as_string());
		case CommandType::GROUP_JOIN: 		return std::format("Joined {}", this->data["joined"].as_string());
		case CommandType::GROUP_LEAVE: 		return std::format("Left {}", this->data["left"].as_string());
		case CommandType::USE: 				return std::format("Used {}:\n{}", this->data["used"].as_string(), this->data["effect"].toString());

		case CommandType::CONNECT:
		case CommandType::QUIT:
		case CommandType::GROUP_CHAT:
		case CommandType::ROOM_CHAT:
		case CommandType::GLOBAL_CHAT:
		case CommandType::WAIT_HANDSHAKE:
		case CommandType::CREATE_PLAYER: 	return "OK";

		default:
			assert(false and "command not mapped for response print");
			return "";
	}
}

std::string	Message::toStringEvent(void) const
{
	assert(this->type == MessageType::EVENT and "wrong type to stringify");

	switch (this->getEventType())
	{
		case EventType::PRESENCE_ENTER:				return std::format("{} entered the room", this->data.as_string());
		case EventType::PRESENCE_LEAVE:				return std::format("{} left the room", this->data.as_string());
		case EventType::ROOM_CHAT:					return std::format("[room] {} says: {}", this->data["player"].as_string(), this->data["text"].as_string());
		case EventType::GROUP_CHAT:					return std::format("[group] {} says: {}", this->data["player"].as_string(), this->data["text"].as_string());
		case EventType::GLOBAL_CHAT:				return std::format("[global] {} says: {}", this->data["player"].as_string(), this->data["text"].as_string());
		case EventType::GROUP_JOIN:					return std::format("{} has joined {}", this->data["player"].as_string(), this->data["group"].as_string());
		case EventType::GROUP_LEAVE:				return std::format("{} has left {}", this->data["player"].as_string(), this->data["group"].as_string());
		case EventType::GROUP_INVITE:				return std::format("{} has been invited to join {}", this->data["player"].as_string(), this->data["group"].as_string());
		case EventType::STATS:						return std::format("Player count {}", this->data.as_int());
		case EventType::ROOM_COMBAT:				return std::format("{} attacked {}", this->data["player"].as_string(), this->data["npc"].as_string());
		case EventType::ROOM_COMBAT_DEATH_NPC:		return std::format("{} slained NPC {}", this->data["player"].as_string(), this->data["npc"].as_string());
		case EventType::ROOM_COMBAT_DEATH_PLAYER:	return std::format("NPC {} slained {}", this->data["npc"].as_string(), this->data["player"].as_string());
		case EventType::ROOM_ITEM_USE:				return std::format("{} used {}", this->data["player"].as_string(), this->data["item"].as_string());
		case EventType::ROOM_QUEST_COMPLETE:		return std::format("{} completed {}", this->data["player"].as_string(), this->data["quest"].as_string());
		default:
			assert(false and "event not mapped for print");
			return "";
	}

}

std::string	Message::toStringError(void) const
{
	assert(this->type == MessageType::ERROR and "wrong type to stringify");

	return this->data["info"].as_string();
}

CommandType	Message::getCommandType(void) const noexcept
{
	assert(this->type == MessageType::COMMAND and ("requested command type but message is not command"));
	return this->subType.commandType;
}

EventType Message::getEventType(void) const noexcept
{
	assert(this->type == MessageType::	EVENT and ("requested event type but message is not event"));
	return this->subType.eventType;
}

ErrorType Message::getErrorType(void) const noexcept
{
	assert(this->type == MessageType::ERROR and ("requested error type but message is not error"));
	return this->subType.errType;
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

void Message::parseCommand(std::string const& command)
{
	this->type = MessageType::COMMAND;

	std::istringstream	iss(command);
	std::string	cmd, secondCmd, data;

	if (!(iss >> cmd))
		throw AppException(ErrorCode::BAD_MESSAGE, "Empty message: " + command);

	if ((cmd == "GROUP") or (cmd == "CHAT") or (cmd == "WAIT"))		// there are command with two words
	{
		if (!(iss >> secondCmd))
			throw AppException(ErrorCode::BAD_MESSAGE, "Incomplete command: " + command);
		cmd += " " + secondCmd;
	}

	if (commandExists(cmd) == false)
		throw AppException(ErrorCode::BAD_MESSAGE, "Unrecognised command: " + command);
	this->subType.commandType = getCommand(cmd);

	if (commandHasArgs(this->subType.commandType) == false)
		return;

	std::getline(iss >> std::ws, data);
	if (data.empty() == true)
		throw AppException(ErrorCode::BAD_MESSAGE, "Missing data for command: " + command);

	if ((data[0] != '{') and (data[0] != '['))		// in this case data is just a string
		data = std::format("\"{}\"", data);
	this->storeJson(data);
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

	if ((data[0] != '{') and (data[0] != '['))		// in this case data is just a string
		data = std::format("\"{}\"", data);
	this->storeJson(data);
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
		this->subType.eventType = getEvent(visibility + " " + type);
		std::getline(iss >> std::ws, data);
		if (data.empty() == true)
			throw AppException(ErrorCode::BAD_MESSAGE, "Missing data for event: " + event);
	}

	if ((data[0] != '{') and (data[0] != '['))		// in this case data is just a string
		data = std::format("\"{}\"", data);
	this->storeJson(data);
}

void Message::parseError(std::string const& error)
{
	this->type = MessageType::ERROR;

	std::istringstream	iss(error);
	uint16_t			errCode;
	std::string			errName, errInfo;

	if (!(iss >> errCode >> errName))
		throw AppException(ErrorCode::BAD_MESSAGE, "Incomplete error: " + error);
	this->subType.errType = getError(errCode);

	std::getline(iss >> std::ws, errInfo);
	if (errInfo.empty() == true)
		throw AppException(ErrorCode::BAD_MESSAGE, "Missing error info: " + error);

	this->storeJson(std::format("{{\"info\": \"{}\"}}", errInfo));
}

void Message::storeJson(std::string const& jsonStr)
{
	std::string error;
	if (parse_json(jsonStr, this->data, error) == false)
		throw AppException(ErrorCode::BAD_MESSAGE, std::format("Error '{}' while parsing: {}", error, jsonStr));
}
