#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>

#include "JsonParser.hpp"
#include "MessageTypes.hpp"


enum class MessageType : uint8_t
{
	COMMAND = 0U,
	RESPONSE,
	ERROR,
	EVENT
};

union MessageSubType
{
	CommandType commandType;
	EventType	eventType;
	ErrorType	errType;
};

static constexpr const char*	S_OK = "OK";
static constexpr const char*	S_ERR = "ERR";
static constexpr const char*	S_EVT = "EVT";

class Message
{
	public:
		explicit Message(std::string const& message);

		Message(Message const& other) = default;
		Message& operator=(Message const& other) = default;
		Message(Message&& other) = default;
		Message& operator=(Message&& other) = default;

		~Message(void) noexcept = default;

		bool isCommand(void) const noexcept { return this->type == MessageType::COMMAND; }
		bool isResponse(void) const noexcept { return this->type == MessageType::RESPONSE; }
		bool isError(void) const noexcept { return this->type == MessageType::ERROR; }
		bool isEvent(void) const noexcept { return this->type == MessageType::EVENT; }

		std::string const&	getRawMessage(void) const noexcept { return this->raw; }
		std::string			toStringCommand(void) const noexcept;
		std::string			toStringResponse(CommandType command) const;
		std::string			toStringEvent(void) const;
		std::string			toStringError(void) const;

		CommandType			getCommandType(void) const noexcept;
		EventType			getEventType(void) const noexcept;
		ErrorType			getErrorType(void) const noexcept;

	private:
		void parse(std::string const& message);
		void parseCommand(std::string const& payload);
		void parseResponse(std::string const& payload);
		void parseEvent(std::string const& payload);
		void parseError(std::string const& payload);

		void storeJson(std::string const& jsonStr);

		std::string		raw;
		MessageType		type;
		MessageSubType	subType;
		JsonValue		data;
};