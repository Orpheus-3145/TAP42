#pragma once

#include <cstdint>
#include <string>
#include <cassert>
#include <vector>
#include <memory>
#include <ostream>
#include <poll.h>

#include "Config.hpp"
#include "Exceptions.hpp"


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
};

std::string toString(GamePhase phase);

std::ostream& operator<<(std::ostream& out, GamePhase phase);

class UI
{
	public:
		UI(int32_t clientSocket) noexcept;

		UI(UI const& other) = delete;
		UI& operator=(UI const& other) = delete;
		UI(UI&& other) = delete;
		UI& operator=(UI&& other) = delete;

		virtual ~UI(void) noexcept {};

		virtual void start(void) { this->keepAlive = true; }
		virtual void stop(void) noexcept { this->keepAlive = false; }

	protected:
		void writeToServer(void);
		void readFromServer(void);
		void handleServerData(std::string const& message);
		void doHandshake(void) noexcept;
		void splitIntoMessages(void);
		void handlePollError(void);

		virtual void switchWindow(GamePhase newPhase);
		virtual void handleCommand(void) = 0;
		virtual void handleResize(void) = 0;
		virtual void handleError(ErrorCode const& code, std::string const& errorInfo) = 0;
		virtual void handleServerDisconnect(void) noexcept = 0;

		virtual void updateResponse(std::string const& response) noexcept = 0;
		virtual void updateEvent(std::string const& event) noexcept = 0;

		static constexpr size_t POLL_SIZE = 1UL;
		static constexpr size_t CLIENT = 0UL;

		std::vector<struct pollfd>	pollFds;

		bool keepAlive{false};

		int32_t height{0};
		int32_t width{0};

		GamePhase	phase{GamePhase::LOGIN};
		bool		handShakeDone{false};

		size_t	toServerSize{0UL};
		char	toServerBuffer[Config::BUFF_SIZE];

		size_t	fromServerSize{0UL};
		char	fromServerBuffer[Config::BUFF_SIZE];
};

std::unique_ptr<UI> uiFactory(int32_t clientSocket);