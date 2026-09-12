#include "UI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cstring>
#include <cstdint>
#include <format>


void UI::readDataFromServer(void)
{
	try
	{
		ssize_t n = ioUtils::readNonBlock(this->clientSocket, this->serverBuffer + this->bufferSize, Config::BUFF_SIZE - this->bufferSize);
		if (n == -1)
		{
			LOG_WARN(LogContext::INTERFACE, "Client unexpectedly terminated connection, closing session");
			this->stopUI();		// or throw?
			return;
		}
		this->bufferSize += n;
		this->handleServerInput();
	}
	catch(const IOException& e)
	{
		LOG_ERROR(LogContext::INTERFACE, std::format("I/O error failed to read from client: '{}'", e.what()));
		this->stopUI();		// or throw?
	}
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

bool UI::writeDataToServer(std::string const& command)
{
	LOG_INFO(LogContext::INTERFACE, "Got new command: " + command);

	// parse command into HTTP request
	std::string request = command + "\n";
	try
	{
		if (ioUtils::writeNonBlock(this->clientSocket, request.data(), request.size()) == -1)
		{
			LOG_WARN(LogContext::INTERFACE, "Client socket busy, trying again later");
			return false;
		}

		if (command == G_QUIT)
			this->stopUI();		// should tell that specifically to graceful terminate after game session
	}
	catch(const IOException& e)
	{
		LOG_ERROR(LogContext::INTERFACE, std::format("I/O error failed to write to client: '{}'", e.what()));
		this->stopUI();		// or throw?
	}
	return true;
}
