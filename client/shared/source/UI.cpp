#include "UI.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

#include <cstring>
#include <format>


void UI::writeInputToServer(void)
{
	if (this->handShakeDone == false)
		throw AppException(ErrorCode::UI_HANDSHAKE_NOT_DONE);
		
	ssize_t n = ioUtils::writeNonBlock(this->clientSocket, this->toServerBuffer, this->toServerSize);
	if (n < 0L)
		throw AppException(ErrorCode::CLIENT_DISCONNECTED);	// NB set pollFD to POLLHUP
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

void UI::readInputFromServer(void)
{
	ssize_t n = ioUtils::readNonBlock(this->clientSocket, this->fromServerBuffer + this->fromServerSize, Config::BUFF_SIZE - this->fromServerSize);
	if (n < 0L)
		throw AppException(ErrorCode::CLIENT_DISCONNECTED);	// NB set pollFD to POLLHUP
	else if (n > 0L)
	{
		std::string serverData = escapeNewLine(this->fromServerBuffer + this->fromServerSize, n);
		LOG_DEBUG(LogContext::INTERFACE, std::format("Read from client: '{}'", serverData));

		this->fromServerSize += n;
		this->splitIntoMessages();
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
				this->gamePhase();
			else if (message.find(S_ERR) == 0UL)
				throw AppException(ErrorCode::UI_USERNAME_NOT_EXISTS);
			else
				LOG_WARN(LogContext::INTERFACE, std::format("Unrecognized message: '{}'", message));
			break;

		case GamePhase::PLAYER_CREATE:
			if (message == LOGIN_OK)
				this->gamePhase();
			else if (message.find(S_ERR) == 0UL)
				throw AppException(ErrorCode::SERVER_ERROR, message);
			else
				LOG_WARN(LogContext::INTERFACE, std::format("Unrecognized message: '{}'", message));
			break;

		case GamePhase::GAME:
			if (message == QUIT_RESPONSE)
				this->stop();
			else if ((message.find(S_OK) == 0UL) or (message.find(S_ERR) == 0UL))
				this->showResponse(message);
			else if (message.find(S_EVT) == 0UL)
				this->showEvent(message);
			else
				LOG_WARN(LogContext::INTERFACE, std::format("Unrecognized message: '{}'", message));
			break;
		
		case GamePhase::ERROR:
			if (message.find(S_ERR) == 0UL)
				LOG_ERROR(LogContext::INTERFACE, std::format("Got error: '{}' while handling a previous error", message));
			else if ((message.find(S_OK) == 0UL) or (message.find(S_EVT) == 0UL))
				{ /* NB handle data to game window, but first detatch printing stuff in tabs to adding to the state */}
			else
				LOG_WARN(LogContext::INTERFACE, std::format("Unrecognized message: '{}'", message));
			break ;

		default:
			break;
	}
}

void UI::doHandshake(void) noexcept
{
	this->handShakeDone = true;
	LOG_DEBUG(LogContext::INTERFACE, std::format("Handshake performed: {}", INITIAL_GREETING));
}

void UI::loginPhase(void)
{
	this->phase = GamePhase::LOGIN;
}

void UI::newPlayerPhase(void)
{
	if (this->handShakeDone == false)
		throw AppException(ErrorCode::UI_HANDSHAKE_NOT_DONE, "Can't create new character");

	LOG_DEBUG(LogContext::INTERFACE, "Creating new player");
	this->phase = GamePhase::PLAYER_CREATE;
}

void UI::gamePhase(void)
{
	if (this->handShakeDone == false)
		throw AppException(ErrorCode::UI_HANDSHAKE_NOT_DONE, "Can't login");

	LOG_DEBUG(LogContext::INTERFACE, "Login successful, retrieving game session");
	this->phase = GamePhase::GAME;
}

void UI::handleError(ErrorCode const& code, std::string const& errorInfo)
{
	LOG_ERROR(LogContext::INTERFACE, mapError(code, errorInfo));
	this->phase = GamePhase::ERROR;
}
