#include "UI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cstring>
#include <cstdint>
#include <format>


void UI::readDataFromServer(int32_t fd)
{
	ssize_t n = ioUtils::readNonBlock(fd, this->serverBuffer + this->bufferSize, Config::BUFF_SIZE - this->bufferSize);
	if (n == -1)
	{
		LOG_WARN(LogContext::INTERFACE, "Client unexpectedly terminated connection, closing session");
		this->exitLoop();
		return;
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

		// NB better input parsing
		ssize_t lenMsg = endMsg - startMsg;
		std::string serverInput = std::string(startMsg, lenMsg);
		LOG_DEBUG(LogContext::INTERFACE, std::format("Received from server: '{}'", serverInput));

		if ((serverInput.find(S_OK) == 0UL) or (serverInput.find(S_ERR) == 0UL))
			this->handleResponse(serverInput);
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

bool UI::forwardDataToServer(int32_t fd, std::string const& command)
{
	LOG_INFO(LogContext::INTERFACE, "Got new command: " + command);

	if (ioUtils::writeNonBlock(fd, command.data(), command.size()) == -1)
	{
		LOG_WARN(LogContext::INTERFACE, "Client socket busy, trying again later");
		return false;
	}

	if (command == G_QUIT)
		this->exitLoop();		// should tell that specifically to graceful terminate after game session
	return true;
}
