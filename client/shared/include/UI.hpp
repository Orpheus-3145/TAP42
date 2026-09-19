#pragma once

#include <cstdint>
#include <string>
#include <cassert>
#include <memory>

#include "Config.hpp"


static constexpr const char*	S_OK = "OK";
static constexpr const char*	S_ERR = "ERR";
static constexpr const char*	S_EVT = "EVT";
static constexpr const char*	QUIT_RESPONSE = "OK bye";
static constexpr const char*	LOGIN_OK = "OK connected";
static constexpr const char*	INITIAL_GREETING = "OK hello proto=1";
static constexpr const char*	CMD_CONNECT = "CONNECT";
static constexpr const char		COMMAND_TERM = '\n';
static constexpr const char		COMMAND_SP = ' ';


enum class GamePhase : uint32_t
{
	HANDSHAKE = 0U,
	LOGIN = 1U,
	PLAYER_CREATE = 2U,
	GAME = 3U,
	ERROR = 4U,
};

class UI
{
	public:
		UI(int32_t clientSocket) noexcept :
			clientSocket{clientSocket}
		{ assert(clientSocket != -1 and "invalid client socket"); }
	
		UI(UI const& other) = delete;
		UI& operator=(UI const& other) = delete;
		UI(UI&& other) = delete;
		UI& operator=(UI&& other) = delete;

		virtual ~UI(void) noexcept {};

		virtual void start(void) = 0;
		virtual void stop(void) noexcept = 0;

	protected:
		void writeInputToServer(void);
		void readInputFromServer(void);
		void handleServerData(std::string const& message);
		void formatCommand(void) noexcept;

		virtual void loginPhase(void);
		virtual void newPlayerPhase(void);
		virtual void gamePhase(void);
		virtual void handleError(std::string const& errMsg) noexcept;

		virtual void handleServerDisconnect(void) noexcept = 0;
		virtual void handleResponse(std::string const& response) noexcept = 0;
		virtual void handleEvent(std::string const& event) noexcept = 0;

		void splitIntoMessages(void);

		int32_t clientSocket;

		GamePhase phase{GamePhase::HANDSHAKE};

		size_t	toServerSize{0UL};
		char	toServerBuffer[Config::BUFF_SIZE];

		size_t	fromServerSize{0UL};
		char	fromServerBuffer[Config::BUFF_SIZE];
};

std::unique_ptr<UI> uiFactory(int32_t clientSocket);