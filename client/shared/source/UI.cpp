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
		case GamePhase::ERROR:			return "error";
		default:						return "";
	}
}

std::ostream& operator<<(std::ostream& out, GamePhase phase)
{
	out << toString(phase);
	return out;
}


UI::UI(void) noexcept :
	gameClientSockets{ioUtils::createSocketPair()},
	commandPipe{ioUtils::createPipe()}
{
	this->pollFds.resize(UI::POLL_SIZE);
	this->pollFds[UI::CLIENT].fd = this->gameClientSockets.first;
	this->pollFds[UI::CLIENT].events = POLLIN;
	this->pollFds[UI::CMD].fd = this->commandPipe.out;	// to send a full command to client
	this->pollFds[UI::CMD].events = POLLIN;

	LOG_DEBUG(LogContext::INTERFACE, std::format("Listening to client socket: {}", this->pollFds[UI::CLIENT].fd));
}

UI::~UI(void) noexcept
{
	if (this->clientHTTP)
		this->clientHTTP->stopWorker();

	ioUtils::closePair(this->gameClientSockets);
	ioUtils::closePipe(this->commandPipe);
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
	if (message.find(S_OK) == 0UL)		// NB make msg struct, stores: type(enum), payload (if any, string/json) also make dispatch function that depends on phase and msg type?
	{
		if (message == INITIAL_GREETING)
			this->shakeHands();
		else if (message == QUIT_RESPONSE)
			this->stop();
		else if (message == LOGIN_OK)
			this->switchWindow(GamePhase::GAME);
		else if (this->phase == GamePhase::GAME)
			this->handleResponse(message);
	}
	else if (message.find(S_EVT) == 0UL)
	{
		if ((this->phase == GamePhase::GAME) or (this->phase == GamePhase::ERROR))
			this->handleEvent(message);
	}
	else if (message.find(S_ERR) == 0UL)
	{
		if (message == ERR_SERVER_DISC)
			throw AppException(ErrorCode::SERVER_DISCONNECTED);
		else if (this->phase == GamePhase::ERROR)
			throw AppException(ErrorCode::SERVER_ERROR, message);
		else if (this->phase == GamePhase::LOGIN)
			throw AppException(ErrorCode::UI_USERNAME_NOT_EXISTS);
		else if (this->phase == GamePhase::PLAYER_CREATE)
			throw AppException(ErrorCode::UI_USERNAME_IN_USE);
		else if (this->phase == GamePhase::GAME)
			this->handleResponse(message);
	}
	else
	{
		LOG_WARN(LogContext::INTERFACE, std::format("Unrecognized message: '{}'", message));
	}
}

void UI::shakeHands(void) noexcept
{
	this->connEstablished = true;
	LOG_DEBUG(LogContext::INTERFACE, std::format("Handshake performed: {}", INITIAL_GREETING));

	if (this->phase == GamePhase::LOGIN)				// fresh new conn
		this->switchWindow(GamePhase::LOGIN);
	else if (this->phase == GamePhase::ERROR)			// disconnected, restore previous session
	{
		std::string loginCommand = std::format("{} {}", CMD_CONNECT, this->username);
		// forward the previously used username to server
		ioUtils::write(this->commandPipe.in, loginCommand.data(), loginCommand.size());
	}
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

void UI::handleError(ErrorData& error)
{
	LOG_ERROR(LogContext::INTERFACE, mapError(error));

	if (error.code == ErrorCode::SERVER_DISCONNECTED)
		this->connEstablished = false;
}

void UI::switchWindow(GamePhase newPhase)
{
	if ((this->connEstablished == false) and (newPhase != GamePhase::ERROR))
		throw AppException(ErrorCode::UI_HANDSHAKE_NOT_DONE, "Can't proceed to new phase without server handshake");

	if (newPhase == GamePhase::ERROR)				// error win: keep listening server data but stop talking
		this->pollFds[UI::CLIENT].events = POLLIN;
	else if (this->toServerSize > 0UL)				// if recovering from error and there's data left, start talking
		this->pollFds[UI::CLIENT].events |= POLLOUT;

	this->phase = newPhase;
}

void UI::handleCommand(void)
{
	char buffer[Config::BUFF_SIZE];
	ssize_t n = ioUtils::read(this->pollFds[UI::CMD].fd, buffer, Config::BUFF_SIZE);

	if ((this->phase == GamePhase::LOGIN) or (this->phase == GamePhase::PLAYER_CREATE))
	{
		this->username = std::string(buffer, n);
		// move to the right to insert CMD_CONNECT and a space at the beginning of the command
		::memmove(buffer + ::strlen(CMD_CONNECT) + 1, buffer, n);
		::memcpy(buffer + ::strlen(CMD_CONNECT), &COMMAND_SP, 1);
		::memcpy(buffer, CMD_CONNECT, ::strlen(CMD_CONNECT));
		n += ::strlen(CMD_CONNECT) + 1;
	}
	buffer[n++] = COMMAND_TERM;

	// store formatted command, ready to be sent to client
	::memcpy(this->toServerBuffer + this->toServerSize, buffer, n);
	this->toServerSize += n;
	this->pollFds[UI::CLIENT].events |= POLLOUT;
}
