#include "UI.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

#include <cstring>
#include <cassert>
#include <format>


std::string toString(GamePhase phase)
{
	switch (phase)
	{
		case GamePhase::LOGIN: 			return "login";
		case GamePhase::PLAYER_CREATE:	return "character creation";
		case GamePhase::GAME:			return "game";
		default:						return "";
	}
}

std::ostream& operator<<(std::ostream& out, GamePhase phase)
{
	out << toString(phase);
	return out;
}


UI::UI(void) noexcept :
	gameClientSockets{ioUtils::createSocketPair()}
{
	this->pollFds.resize(UI::POLL_SIZE);
	this->pollFds[UI::CLIENT].fd = this->gameClientSockets.first;
	this->pollFds[UI::CLIENT].events = POLLIN;

	LOG_DEBUG(LogContext::INTERFACE, std::format("Listening to client socket: {}", this->pollFds[UI::CLIENT].fd));
}

UI::~UI(void) noexcept
{
	if (this->clientHTTP) this->clientHTTP->stopWorker();
	ioUtils::closePair(this->gameClientSockets);
}

void UI::connect(std::string host, uint32_t port)
{
	this->clientHTTP = std::make_unique<ClientHTTP>(host, port, this->gameClientSockets.second);
}

void UI::start(void)
{
	this->keepAlive = true;
	this->clientHTTP->startWorker();
}

void UI::stop(void) noexcept
{
	this->keepAlive = false;
	this->clientHTTP->stopWorker();
}

void UI::writeToServer(void)
{
	if (this->connEstablished == false)
		throw AppException(ErrorCode::UI_HANDSHAKE_NOT_DONE);

	int32_t clientSocket = this->pollFds[UI::CLIENT].fd;
	ssize_t n = ioUtils::writeNonBlock(clientSocket, this->toServerBuffer, this->toServerSize);

	if (n < 0L)		// disconnected
		this->pollFds[UI::CLIENT].revents = POLLHUP;
	else if (n > 0L)	
	{
		std::string gameData = escapeNewLine(this->toServerBuffer, n);
		LOG_DEBUG(LogContext::INTERFACE, std::format("Sent to client: '{}'", gameData));

		if (std::string(this->toServerBuffer, n - 1) == "quit")		// NB only for debugging purpuses 
			this->stop();
		this->toServerSize -= n;
		if (this->toServerSize > 0UL)		// move the remaining data to send at the beginning of the buffer
			::memmove(this->toServerBuffer, this->toServerBuffer + n, this->toServerSize);
		else
			this->pollFds[UI::CLIENT].events = POLLIN;		// written everything, just listen now
	}
	else
		LOG_WARN(LogContext::INTERFACE, "Client socket buffer is busy, try again later");
}

void UI::readFromServer(void)
{
	int32_t clientSocket = this->pollFds[UI::CLIENT].fd;
	ssize_t n = ioUtils::readNonBlock(clientSocket, this->toGameBuffer + this->toGameSize, Config::BUFF_SIZE - this->toGameSize);

	if (n < 0L)		// disconnected
		this->pollFds[UI::CLIENT].revents = POLLHUP;
	else if (n > 0L)
	{
		std::string serverData = escapeNewLine(this->toGameBuffer + this->toGameSize, n);
		LOG_DEBUG(LogContext::INTERFACE, std::format("Read from client: '{}'", serverData));

		this->toGameSize += n;
		this->splitIntoMessages();
	}
	else
		LOG_WARN(LogContext::INTERFACE, "Client socket buffer is full, send data to UI and empty it");
}

void UI::splitIntoMessages(void)
{
	char *startMsg = this->toGameBuffer, *endMsg = nullptr;
	while (true)
	{
		endMsg = reinterpret_cast<char*>(::memchr(startMsg, COMMAND_TERM, this->toGameSize));
		if (endMsg == nullptr)
			break;

		ssize_t lenMsg = endMsg - startMsg;
		std::string message = std::string(startMsg, lenMsg);
		startMsg += lenMsg + 1UL;
		this->toGameSize -= lenMsg + 1UL;

		try
		{
			this->handleMessage(message);
		}
		catch(AppException const& e)
		{
			if (this->toGameSize > 0UL)
				::memmove(this->toGameBuffer, startMsg, this->toGameSize);
			throw e;
		}
	}
	if (this->toGameSize > 0UL)
		::memmove(this->toGameBuffer, startMsg, this->toGameSize);
}

void UI::handleMessage(std::string const& message)
{
	if (message.find(S_OK) == 0UL)		// NB make msg struct, stores: type(enum), payload(if any, string/json) also make dispatch function
	{
		if (message == INITIAL_GREETING)
			this->doHandshake();
		else if (message == QUIT_RESPONSE)
			this->stop();
		else if (message == LOGIN_OK)
			this->switchWindow(GamePhase::GAME);
		else if (this->phase == GamePhase::GAME)
			this->updateResponse(message);
	}
	else if (message.find(S_EVT) == 0UL)
	{
		if (this->phase == GamePhase::GAME)
			this->updateEvent(message);
	}
	else if (message.find(S_ERR) == 0UL)
	{
		if (message == ERR_SERVER_DISC)
			throw AppException(ErrorCode::SERVER_DISCONNECTED);
		else if (this->phase == GamePhase::LOGIN)
			throw AppException(ErrorCode::UI_USERNAME_NOT_EXISTS);
		else if (this->phase == GamePhase::PLAYER_CREATE)
			throw AppException(ErrorCode::UI_USERNAME_IN_USE);
		else if (this->phase == GamePhase::GAME)
			this->updateResponse(message);
	}
	else
	{
		LOG_WARN(LogContext::INTERFACE, std::format("Unrecognized message: '{}'", message));
	}
}

void UI::doHandshake(void) noexcept
{
	this->connEstablished = true;
	LOG_DEBUG(LogContext::INTERFACE, std::format("Handshake performed: {}", INITIAL_GREETING));

	if (this->phase == GamePhase::LOGIN)				// fresh new conn or disconnected during login
		this->switchWindow(GamePhase::LOGIN);
	else if (this->phase == GamePhase::PLAYER_CREATE)	// disconnected during character creation
		this->switchWindow(GamePhase::PLAYER_CREATE);
	else if (this->phase == GamePhase::GAME)			// disconnected during game
		this->addMessageToServerQueue(std::format("{} {}", CMD_CONNECT, this->username));
}

void UI::addMessageToServerQueue(std::string const& msg, bool forceInsert) noexcept	// NB write inside pipe and trigger CLI::handleCommand()
{
	// add an error message to the queue of msg to send to game
	if ((this->toServerSize + msg.size()) > Config::BUFF_SIZE)
	{
		if (forceInsert == false)
			return;

		// if buffer is full, truncate current message to make room for the message
		this->toServerSize = Config::BUFF_SIZE - msg.size() - 2;
		this->toServerBuffer[this->toServerSize++] = COMMAND_TERM;
	}
	::memcpy(this->toServerBuffer + this->toServerSize, msg.data(), msg.size());
	this->toServerSize += msg.size();
	this->toServerBuffer[this->toServerSize++] = COMMAND_TERM;

	this->pollFds[UI::CLIENT].events |= POLLOUT;
}

void UI::addMessageToGameQueue(std::string const& msg, bool forceInsert) noexcept
{
	// add an error message to the queue of msg to send to game
	if ((this->toGameSize + msg.size()) > Config::BUFF_SIZE)
	{
		if (forceInsert == false)
			return;

		// if buffer is full, truncate current message to make room for the message
		this->toGameSize = Config::BUFF_SIZE - msg.size() - 2;
		this->toGameBuffer[this->toGameSize++] = COMMAND_TERM;
	}
	::memcpy(this->toGameBuffer + this->toGameSize, msg.data(), msg.size());
	this->toGameSize += msg.size();
	this->toGameBuffer[this->toGameSize++] = COMMAND_TERM;
}

void UI::handlePollError(void)
{
	if (this->pollFds[CLIENT].revents & POLLHUP)
		throw AppException(ErrorCode::CLIENT_DISCONNECTED);
	else if (this->pollFds[CLIENT].revents & POLLNVAL)
		throw AppException(ErrorCode::IO_POLL_FAILED, "invalid socket (POLLNVAL)");
	else if (this->pollFds[CLIENT].revents & POLLERR)	// something actually went wrong with poll
	{
		int32_t sockErr = 0;
		socklen_t len = sizeof(sockErr);
	
		if (ioUtils::getsockopt(this->pollFds[UI::CLIENT].fd, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, std::format("getsockopt failed during POLLERR: {}", ::strerror(errno)));
		else if (sockErr != 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, ::strerror(sockErr));
	}
}

void UI::handleError(ErrorCode const& code, std::string const& errorInfo)
{
	LOG_ERROR(LogContext::INTERFACE, errorInfo);

	this->pollFds[UI::CLIENT].events = POLLIN;		// keep receiving server data but stop talking
	if (code == ErrorCode::SERVER_DISCONNECTED)
		this->connEstablished = false;
}

void UI::switchWindow(GamePhase newPhase)
{
	if (this->connEstablished == false)
		throw AppException(ErrorCode::UI_HANDSHAKE_NOT_DONE, "Can't proceed to new phase without server handshake");

	LOG_DEBUG(LogContext::INTERFACE, std::format("Starting {} session", toString(newPhase)));
	this->phase = newPhase;
}
