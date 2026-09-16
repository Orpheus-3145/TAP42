#include "UI.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cstring>
#include <format>


void UI::handleInputToServer(void)
{
	try
	{
		ssize_t n = ioUtils::writeNonBlock(this->clientSocket, this->toServerBuffer, this->toServerSize);

		if (n < 0L)
		{
			LOG_WARN(LogContext::HTTP_CLIENT, "Client socket disconnected");
			this->handleError("Client socket disconnected");
		}
		else if (n > 0L)	
		{
			std::string gameData = escapeNewLine(this->toServerBuffer, n);
			LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Sent to client: '{}'", gameData));

			if (std::string(this->toServerBuffer, n) == "quit")
				this->stop();
			this->toServerSize -= n;
			if (this->toServerSize > 0UL)
				::memmove(this->toServerBuffer, this->toServerBuffer + n, this->fromServerSize);
		}
		else
			LOG_WARN(LogContext::INTERFACE, "Client socket buffer is busy, try again later");

	}
	catch(const IOException& e)
	{
		std::string errMsg = std::format("I/O error failed to write to client: '{}'", e.what());
		LOG_ERROR(LogContext::INTERFACE, errMsg);
		this->handleError(errMsg);
	}
	if (std::string(this->toServerBuffer, this->toServerSize) == "quit")
		this->stop();
}

void UI::handleInputFromServer(void)
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
		std::string errMsg = std::format("I/O error failed to read from client: '{}'", e.what());
		LOG_ERROR(LogContext::INTERFACE, errMsg);
		this->handleError(errMsg);
	}
}

void UI::splitIntoMessages(void)
{
	char *startMsg = this->fromServerBuffer, *endMsg = nullptr;
	while (true)
	{
		endMsg = reinterpret_cast<char*>(::memchr(startMsg, Config::COMMAND_TERM, this->fromServerSize));
		if (endMsg == nullptr)
			break;

		ssize_t lenMsg = endMsg - startMsg;
		std::string serverInput = std::string(startMsg, lenMsg);

		if ((serverInput.find(S_OK) == 0UL) or (serverInput.find(S_ERR) == 0UL))
		{
			if (serverInput == QUIT_RESPONSE)
			{
				this->stop();
				return;
			}
			this->handleResponse(serverInput);
		}
		else if (serverInput.find(S_EVT) == 0UL)
			this->handleEvent(serverInput);
		else
			LOG_WARN(LogContext::INTERFACE, std::format("Unknown input"));

		startMsg += lenMsg + 1UL;
		this->fromServerSize -= lenMsg + 1UL;
	}
	if (this->fromServerSize > 0UL)
		::memmove(this->fromServerBuffer, startMsg, this->fromServerSize);
}
