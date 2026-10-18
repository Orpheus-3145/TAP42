#pragma once

#include <cstdint>
#include <string>
#include <cassert>
#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <memory>
#include <ostream>
#include <optional>
#include <poll.h>

#include "Config.hpp"
#include "ClientHTTP.hpp"
#include "Message.hpp"
#include "Exceptions.hpp"
#include "Utils.hpp"


enum class GamePhase : uint32_t
{
	ND = 0U,
	LOGIN,
	PLAYER_CREATE,
	GAME,
	ERROR
};

std::string toString(GamePhase phase);

std::ostream& operator<<(std::ostream& out, GamePhase phase);

static std::vector<std::string> CMD_HINTS
{
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
	"USE"
};

static std::vector<std::string> CHAT_CMD_HINTS
{
	"GROUP CREATE",
	"GROUP INVITE",
	"GROUP JOIN",
	"GROUP LEAVE",
	"CHAT GROUP",
	"CHAT ROOM",
	"CHAT GLOBAL",
	"QUIT"
};

class UI
{
	using MessageDispatcher = std::unordered_map<GamePhase,std::function<void(Message const&)>>;

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
		virtual void switchWindow(std::optional<GamePhase> newPhase = std::nullopt);
		std::string const& getUsername(void) const noexcept { return this->username; }

	protected:
		virtual ssize_t writeToServer(void);
		virtual ssize_t readFromServer(void);

		void splitIntoMessages(void);
		void dispatchMessage(std::string const& message);

		virtual void handleCommand(std::string const& input);

		virtual void shakeHands(void) noexcept;
		virtual void handleException(ErrorData const& error);

		virtual void showResponse(Message const& response) noexcept = 0;
		virtual void showEvent(Message const& event) noexcept = 0;
		virtual void showError(Message const& error) noexcept = 0;

		ioUtils::SocketPair			gameClientSockets;
		std::unique_ptr<ClientHTTP>	clientHTTP;

		std::queue<Message> commandQueue;

		MessageDispatcher responseDispatcher;
		MessageDispatcher eventDispatcher;
		MessageDispatcher errorDispatcher;

		std::string username;

		bool		connEstablished{false};
		GamePhase	phase{GamePhase::ND}, lastPhase{GamePhase::ND};

		size_t	toServerSize{0UL};
		char	toServerBuffer[Config::BUFF_SIZE];

		size_t	toGameSize{0UL};
		char	toGameBuffer[Config::BUFF_SIZE];
};

std::unique_ptr<UI> uiFactory(void);