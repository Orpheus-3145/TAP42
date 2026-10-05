#pragma once

#include <cstdint>
#include <string>
#include <cassert>
#include <vector>
#include <memory>
#include <ostream>
#include <poll.h>

#include "Config.hpp"
#include "ClientHTTP.hpp"
#include "Exceptions.hpp"
#include "Utils.hpp"


inline constexpr const char*	S_OK = "OK";
inline constexpr const char*	S_ERR = "ERR";
inline constexpr const char*	S_EVT = "EVT";
inline constexpr const char*	QUIT_RESPONSE = "OK bye";
inline constexpr const char*	LOGIN_OK = "OK connected";
inline constexpr const char*	INITIAL_GREETING = "OK hello proto=1";
inline constexpr const char*	CMD_CONNECT = "CONNECT";
inline constexpr const char*	ERR_SERVER_DISC = "ERR SERVER DISCONNECTED";
inline constexpr const char		COMMAND_TERM = '\n';
inline constexpr const char		COMMAND_SP = ' ';


enum class GamePhase : uint32_t
{
	ND = 0U,
	LOGIN = 1U,
	PLAYER_CREATE = 2U,
	GAME = 3U,
	ERROR = 4U,
};

std::string toString(GamePhase phase);

std::ostream& operator<<(std::ostream& out, GamePhase phase);

class UI
{
	public:
		UI(void) noexcept;

		UI(UI const& other) = delete;
		UI& operator=(UI const& other) = delete;
		UI(UI&& other) = delete;
		UI& operator=(UI&& other) = delete;

		virtual ~UI(void) noexcept;

		void connect(std::string host, uint32_t port);
		void reconnect(void) const noexcept { this->clientHTTP->wakeUpWorker();}

		virtual void start(void);
		virtual void stop(void) noexcept;
		virtual void switchWindow(GamePhase newPhase);
		std::string const& getUsername(void) const noexcept { return this->username; }

	protected:
		void writeToServer(void);
		void readFromServer(void);
		void splitIntoMessages(void);
		void handleMessage(std::string const& message);
		void handlePollError(void);
		
		virtual void shakeHands(void) noexcept;
		virtual void handleCommand(void);
		virtual void handleResponse(std::string const& response) noexcept = 0;
		virtual void handleEvent(std::string const& event) noexcept = 0;
		virtual void handleError(ErrorData& error);

		static constexpr size_t POLL_SIZE = 2UL;
		static constexpr size_t CLIENT = 0UL;
		static constexpr size_t CMD = 1UL;

		std::vector<struct pollfd>	pollFds;
		ioUtils::SocketPair			gameClientSockets;
		ioUtils::Pipe				commandPipe;
	
		std::unique_ptr<ClientHTTP>	clientHTTP;

		std::string username;

		bool	keepAlive{false};
		bool	connEstablished{false};

		int32_t height{0};
		int32_t width{0};

		GamePhase	phase{GamePhase::LOGIN};

		size_t	toServerSize{0UL};
		char	toServerBuffer[Config::BUFF_SIZE];

		size_t	toGameSize{0UL};
		char	toGameBuffer[Config::BUFF_SIZE];
};

std::unique_ptr<UI> uiFactory(void);