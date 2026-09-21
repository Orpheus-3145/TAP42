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
	LOGIN = 0U,
	PLAYER_CREATE = 1U,
	GAME = 2U,
	ERROR = 3U,
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

		virtual void loginPhase(void);
		virtual void newPlayerPhase(void);
		virtual void gamePhase(void);

		virtual void handleError(std::string const& errMsg) noexcept;
		virtual void handleServerDisconnect(void) noexcept = 0;

		virtual void showResponse(std::string const& response) noexcept = 0;
		virtual void showEvent(std::string const& event) noexcept = 0;

		void splitIntoMessages(void);

		int32_t clientSocket;

		GamePhase	phase{GamePhase::LOGIN};
		bool		handShakeDone{false};

		size_t	toServerSize{0UL};
		char	toServerBuffer[Config::BUFF_SIZE];

		size_t	fromServerSize{0UL};
		char	fromServerBuffer[Config::BUFF_SIZE];
};

std::unique_ptr<UI> uiFactory(int32_t clientSocket);