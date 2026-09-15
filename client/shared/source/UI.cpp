#include "UI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cstring>
#include <cstdint>
#include <format>


void UI::readDataFromServer(void)
{
	ssize_t n = ioUtils::readNonBlock(this->clientSocket, this->serverBuffer + this->bufferSize, Config::BUFF_SIZE - this->bufferSize);
	if (n == -1)
	{
		LOG_WARN(LogContext::INTERFACE, "Client unexpectedly terminated connection, closing session");
		throw IOException("Client unexpectedly terminated connection, closing session");
	}
	this->bufferSize += n;
	this->handleServerInput();
}

void UI::handleServerInput(void)
{
	char *startMsg = this->serverBuffer, *endMsg = nullptr;
	while (true)
	{
		endMsg = reinterpret_cast<char*>(::memchr(startMsg, Config::MSG_TERM, this->bufferSize));
		if (endMsg == nullptr)
			break;

		ssize_t lenMsg = endMsg - startMsg;
		std::string serverInput = std::string(startMsg, lenMsg);
		LOG_DEBUG(LogContext::INTERFACE, std::format("Received from server: '{}'", serverInput));

		if ((serverInput.find(S_OK) == 0UL) or (serverInput.find(S_ERR) == 0UL))
		{
			if (serverInput == QUIT_RESPONSE)
			{
				this->stopUI();
				return;
			}
			this->handleResponse(serverInput);
		}
		else if (serverInput.find(S_EVT) == 0UL)
			this->handleEvent(serverInput);
		else
			LOG_WARN(LogContext::INTERFACE, std::format("Unknown input"));

		startMsg += lenMsg + 1UL;
		this->bufferSize -= lenMsg + 1UL;
	}
	if (this->bufferSize > 0UL)
		::memmove(this->serverBuffer, startMsg, this->bufferSize);
}

bool UI::writeDataToServer(std::string const& command)
{
	LOG_INFO(LogContext::INTERFACE, "Got new command: " + command);

	// parse command into HTTP request
	std::string request = command + "\n";
	if (ioUtils::writeNonBlock(this->clientSocket, request.data(), request.size()) == -1)
	{
		LOG_WARN(LogContext::INTERFACE, "Client socket busy, trying again later");
		return false;
	}
	return true;
}
