#include "UI.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cstring>
#include <format>


void UI::writeInputToServer(void)
{
	this->formatCommand();		// NB doesn't work if multiple commands are inside the buffer
	try
	{
		ssize_t n = ioUtils::writeNonBlock(this->clientSocket, this->toServerBuffer, this->toServerSize);

		if (n < 0L)
			this->handleError("Client socket disconnected");
		else if (n > 0L)	
		{
			std::string gameData = escapeNewLine(this->toServerBuffer, n);
			LOG_DEBUG(LogContext::INTERFACE, std::format("Sent to client: '{}'", gameData));

			if (std::string(this->toServerBuffer, n - 1) == "quit")		// NB only for debugging purpuses 
				this->stop();
			this->toServerSize -= n;
			if (this->toServerSize > 0UL)
				::memmove(this->toServerBuffer, this->toServerBuffer + n, this->toServerSize);
		}
		else
			LOG_WARN(LogContext::INTERFACE, "Client socket buffer is busy, try again later");
	}
	catch(const IOException& e)
	{
		this->handleError(std::format("I/O error failed to write to client: '{}'", e.what()));
	}
}

void UI::readInputFromServer(void)
{
	try
	{
		ssize_t n = ioUtils::readNonBlock(this->clientSocket, this->fromServerBuffer + this->fromServerSize, Config::BUFF_SIZE - this->fromServerSize);
		if (n < 0L)
		{
			LOG_WARN(LogContext::INTERFACE, "Client unexpectedly terminated connection, closing session");
			throw IOException("Client unexpectedly terminated connection, closing session");
		}
		else if (n > 0L)
		{
			std::string serverData = escapeNewLine(this->fromServerBuffer + this->fromServerSize, n);
			LOG_DEBUG(LogContext::INTERFACE, std::format("Read from client: '{}'", serverData));
	
			this->fromServerSize += n;
			this->splitIntoMessages();
		}
	}
	catch(const IOException& e)
	{
		this->handleError(std::format("I/O error failed to read from client: '{}'", e.what()));
	}
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

		this->handleServerData(std::string(startMsg, lenMsg));

		startMsg += lenMsg + 1UL;
		this->fromServerSize -= lenMsg + 1UL;
	}
	if (this->fromServerSize > 0UL)
		::memmove(this->fromServerBuffer, startMsg, this->fromServerSize);
}

void UI::handleServerData(std::string const& message)
{
	if (this->phase == GamePhase::HANDSHAKE)
	{
		if (message == INITIAL_GREETING)
			this->loginPhase();
		else
			LOG_WARN(LogContext::INTERFACE, std::format("Unexpected message: '{}'", message));
	}
	else if (this->phase == GamePhase::LOGIN)
	{
		if (message == LOGIN_OK)
			this->gamePhase();
		else if (message.find(S_ERR) == 0UL)
			this->newPlayerPhase();
			// this->handleError("Username doesn't exist");
		else
			LOG_WARN(LogContext::INTERFACE, std::format("Unexpected message: '{}'", message));
	}
	else if (this->phase == GamePhase::PLAYER_CREATE)
	{
		if (message == LOGIN_OK)
			this->gamePhase();
		else
			LOG_WARN(LogContext::INTERFACE, std::format("Unexpected message: '{}'", message));
	}
	else if (this->phase == GamePhase::GAME)
	{
		if (message == QUIT_RESPONSE)
			this->stop();
		else if ((message.find(S_OK) == 0UL) or (message.find(S_ERR) == 0UL))
			this->handleResponse(message);
		else if (message.find(S_EVT) == 0UL)
			this->handleEvent(message);
		else
			LOG_WARN(LogContext::INTERFACE, std::format("Unexpected message: '{}'", message));
	}
	else
		LOG_WARN(LogContext::INTERFACE, std::format("Unexpected message: '{}'", message));
}

void UI::formatCommand(void) noexcept
{
	if ((this->phase == GamePhase::LOGIN) or (this->phase == GamePhase::PLAYER_CREATE))
	{
		// move to the right to insert CMD_CONNECT at the beginning of the command
		::memmove(this->toServerBuffer + ::strlen(CMD_CONNECT) + 1, this->toServerBuffer, this->toServerSize);
		::memcpy(this->toServerBuffer + ::strlen(CMD_CONNECT), &COMMAND_SP, 1);
		::memcpy(this->toServerBuffer, CMD_CONNECT, ::strlen(CMD_CONNECT));
		this->toServerSize += ::strlen(CMD_CONNECT) + 1;
	}
	::memcpy(this->toServerBuffer + this->toServerSize, &COMMAND_TERM, 1);
	this->toServerSize++;
}

void UI::loginPhase(void)
{
	LOG_DEBUG(LogContext::INTERFACE, "Handshake with server successful, moving to login");
	this->phase = GamePhase::LOGIN;
}

void UI::newPlayerPhase(void)
{
	LOG_DEBUG(LogContext::INTERFACE, "Creating new player");
	this->phase = GamePhase::PLAYER_CREATE;
}

void UI::gamePhase(void)
{
	LOG_DEBUG(LogContext::INTERFACE, "Login successful, retrieving game session");
	this->phase = GamePhase::GAME;
}

void UI::handleError(std::string const& errMsg) noexcept
{
	LOG_ERROR(LogContext::INTERFACE, errMsg);
	this->phase = GamePhase::ERROR;
}