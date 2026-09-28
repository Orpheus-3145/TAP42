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


UI::UI(int32_t clientSocket) noexcept
{
	assert(clientSocket != -1 and "invalid client socket");

	this->pollFds.resize(UI::POLL_SIZE);
	this->pollFds[UI::CLIENT].fd = clientSocket;
}

void UI::writeToServer(void)
{
	if (this->handShakeDone == false)
		throw AppException(ErrorCode::UI_HANDSHAKE_NOT_DONE);

	int32_t clientSocket = this->pollFds[UI::CLIENT].fd;
	ssize_t n = ioUtils::writeNonBlock(clientSocket, this->toServerBuffer, this->toServerSize);

	if (n < 0L)
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
			this->pollFds[UI::CLIENT].events = POLLIN;		// stop writing to server
	}
	else
		LOG_WARN(LogContext::INTERFACE, "Client socket buffer is busy, try again later");
}

void UI::readFromServer(void)
{
	int32_t clientSocket = this->pollFds[UI::CLIENT].fd;
	ssize_t n = ioUtils::readNonBlock(clientSocket, this->fromServerBuffer + this->fromServerSize, Config::BUFF_SIZE - this->fromServerSize);

	if (n < 0L)
		this->pollFds[UI::CLIENT].revents = POLLHUP;
	else if (n > 0L)
	{
		std::string serverData = escapeNewLine(this->fromServerBuffer + this->fromServerSize, n);
		LOG_DEBUG(LogContext::INTERFACE, std::format("Read from client: '{}'", serverData));

		this->fromServerSize += n;
		this->splitIntoMessages();
	}
	else
		LOG_WARN(LogContext::INTERFACE, "Client socket buffer is full, send data to UI and empty it");
}

void UI::splitIntoMessages(void)
{
	char *startMsg = this->fromServerBuffer, *endMsg = nullptr;
	while (true)
	{
		endMsg = reinterpret_cast<char*>(::memchr(startMsg, COMMAND_TERM, this->fromServerSize));
		if (endMsg == nullptr)
			break;

		ssize_t lenMsg = endMsg - startMsg;

		try
		{
			this->handleServerData(std::string(startMsg, lenMsg));
		}
		catch(AppException const& e)
		{
			startMsg += lenMsg + 1UL;
			this->fromServerSize -= lenMsg + 1UL;
			if (this->fromServerSize > 0UL)
				::memmove(this->fromServerBuffer, startMsg, this->fromServerSize);
			throw e;
		}

		startMsg += lenMsg + 1UL;
		this->fromServerSize -= lenMsg + 1UL;
	}
	if (this->fromServerSize > 0UL)
		::memmove(this->fromServerBuffer, startMsg, this->fromServerSize);
}

void UI::handleServerData(std::string const& message)
{
	if (message == INITIAL_GREETING)
	{
		this->doHandshake();
		return;
	}

	switch (this->phase)
	{
		case GamePhase::LOGIN:
			if (message == LOGIN_OK)
				this->switchWindow(GamePhase::GAME);
			else if (message.find(S_ERR) == 0UL)
				throw AppException(ErrorCode::UI_USERNAME_NOT_EXISTS);
			else
				LOG_WARN(LogContext::INTERFACE, std::format("Unrecognized message: '{}'", message));
			break;

		case GamePhase::PLAYER_CREATE:
			if (message == LOGIN_OK)
				this->switchWindow(GamePhase::GAME);
			else if (message.find(S_ERR) == 0UL)
				throw AppException(ErrorCode::SERVER_ERROR, message);
			else
				LOG_WARN(LogContext::INTERFACE, std::format("Unrecognized message: '{}'", message));
			break;

		case GamePhase::GAME:
			if (message == QUIT_RESPONSE)
				this->stop();
			else if ((message.find(S_OK) == 0UL) or (message.find(S_ERR) == 0UL))
				this->updateResponse(message);
			else if (message.find(S_EVT) == 0UL)
				this->updateEvent(message);
			else
				LOG_WARN(LogContext::INTERFACE, std::format("Unrecognized message: '{}'", message));
			break;

		default:
			break;
	}
}

void UI::doHandshake(void) noexcept
{
	this->handShakeDone = true;
	LOG_DEBUG(LogContext::INTERFACE, std::format("Handshake performed: {}", INITIAL_GREETING));
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
	
		int32_t clientSocket = this->pollFds[UI::CLIENT].fd;
		if (ioUtils::getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, std::format("getsockopt failed during POLLERR: {}", ::strerror(errno)));
		else if (sockErr != 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, ::strerror(sockErr));
	}
}

void UI::switchWindow(GamePhase newPhase)
{
	if ((newPhase != GamePhase::LOGIN) and (this->handShakeDone == false))
		throw AppException(ErrorCode::UI_HANDSHAKE_NOT_DONE, "Can't proceed to new phase without having received the handshake from server");

	LOG_DEBUG(LogContext::INTERFACE, std::format("Starting {} session", toString(newPhase)));
	this->phase = newPhase;
}