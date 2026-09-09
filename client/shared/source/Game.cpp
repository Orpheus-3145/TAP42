#include "Game.hpp"
#include "Utils.hpp"
#include "CLI.hpp"
#include "GUI.hpp"
#include "Exceptions.hpp"

#include <cstring>				// strerror, memchr, memeset, memmove
#include <cassert>
#include <functional>
#include <format>

#include <csignal>
#include <sys/ioctl.h>
#include <unistd.h>



void Game::run(std::string const& host, uint32_t port)
{
	LOG_INFO(LogContext::GAME_CLIENT, "Game launched");

	this->dataSize = 0UL;
	this->commandLength = 0UL;

	ioUtils::Pipe commandPipe = ioUtils::createPipe();
	ioUtils::SocketPair gameClientSockets = ioUtils::createSocketPair();

	(void)host;
	(void)port;
	// this->clientHTTP = std::make_unique<ClientHTTP>(host, port);
	// this->clientHTTP->startWorker(gameClientSockets.first);

	// decide if use CLI or GUI
	this->interface = std::make_unique<CLI>(commandPipe.in);
	
	this->startWorker(gameClientSockets.second, commandPipe.out);

	this->interface->loop();		// blocks here, NB if exceptions happen here they must be caught and terminate the running threads
	LOG_INFO(LogContext::GAME_CLIENT, "Ended game loop");
	this->stopWorker();

	// this->clientHTTP->stopWorker();
	// this->clientHTTP->disconnect();

	LOG_INFO(LogContext::GAME_CLIENT, "Game stopped");

	ioUtils::closePipe(commandPipe);
	ioUtils::closePair(gameClientSockets);
}

void Game::startWorker(int32_t clientSocket, int32_t commandFd) noexcept
{
	assert(clientSocket != -1 and "invalid game socket");
	assert(commandFd != -1 and "invalid command file descriptor");

	this->worker = std::thread(&Game::pollLoop, this, clientSocket, commandFd);
	this->keepAlive.store(true);
	LOG_INFO(LogContext::GAME_CLIENT, std::format("Started game worker, listening to UNIX socket: {}", clientSocket));
}

void Game::stopWorker(void) noexcept
{
	this->keepAlive.store(false);

	this->wakeUpWorker();
	if (this->worker.joinable())
	{
		this->worker.join();
		LOG_DEBUG(LogContext::GAME_CLIENT, "Stopped game worker");
	}
}

void Game::wakeUpWorker(void) noexcept
{
	char byte = 'x';
	ioUtils::write(this->wakeupPipe.in, &byte, 1UL);
}

void Game::pollLoop(int32_t clientSocket, int32_t commandPipeInput)
{
	assert(clientSocket != -1 and "invalid client socket");
	assert(commandPipeInput != -1 and "invalid command pipe");

	std::vector<struct pollfd> pollFds(3);
	// main thread calls, only read
	pollFds[0].fd = this->wakeupPipe.out;
	// user commands, only read
	pollFds[1].fd = commandPipeInput;
	// client socket, read and write
	pollFds[2].fd = clientSocket;

	while (this->keepAlive.load())
	{
		pollFds[0].events |= POLLIN;
		pollFds[0].revents = 0;
		pollFds[1].events |= POLLIN;
		pollFds[1].revents = 0;
		pollFds[2].events |= POLLIN;
		pollFds[2].revents = 0;
		ioUtils::poll(pollFds.data(), pollFds.size(), -1);

		if (pollFds[0].revents & POLLIN)	// worker awaken from main thread, flush pipe	NB use it to gracelly close the client when user closes session?
			this->flushPipe();
		
		if (pollFds[1].revents & POLLIN)
			this->readCommandFromUI(pollFds);
		
		if (pollFds[2].revents & POLLIN)
			this->readDataFromServer(clientSocket);

		if (pollFds[2].revents & POLLOUT)
			this->forwardCommandToServer(pollFds);

		// client closed connection (because server did so) (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (pollFds[2].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_WARN(LogContext::INTERFACE, "Client unexpectedly terminated connection, closing session");
			this->keepAlive.store(false);
		}
	}
}

void Game::flushPipe(void) const noexcept
{
	char tmp[64];
	ioUtils::read(this->wakeupPipe.out, tmp, 64);
}

void Game::readCommandFromUI(std::vector<struct pollfd>& pollFds)
{
	this->commandLength = ioUtils::read(pollFds[1].fd, this->commandBuffer, Config::CMD_BUFFER_SIZE);

	LOG_DEBUG(LogContext::GAME_CLIENT, std::format("Received command from UI: '{}'", std::string(this->commandBuffer, this->commandLength)));
	pollFds[2].events |= POLLOUT;
}

void Game::readDataFromServer(int32_t clientSocket)
{
	ssize_t n = 0L;

	do	// while loop because buffer could overflow
	{
		n = ioUtils::readNonBlock(clientSocket, this->serverData + this->dataSize, Config::BUFF_SIZE - this->dataSize);

		if (n > 0L)
		{
			this->dataSize += n;
			this->handleServerInput();
		}
		else if (n == -1L)		// client closed connection
		{
			LOG_WARN(LogContext::GAME_CLIENT, "Client unexpectedly terminated connection, closing session");
			this->keepAlive.store(false);
		}
	} while(n > 0L);
}

void Game::handleServerInput(void)
{
	char *startMsg = this->serverData, *endMsg = nullptr;
	while (true)
	{
		endMsg = reinterpret_cast<char*>(::memchr(startMsg, Config::MSG_TERM, this->dataSize));
		if (endMsg == nullptr)
			break;

		// NB better input parsing
		ssize_t lenMsg = endMsg - startMsg;
		std::string serverInput = std::string(startMsg, lenMsg);
		LOG_DEBUG(LogContext::GAME_CLIENT, std::format("Received from server: '{}'", serverInput));

		if ((serverInput.find(S_OK) == 0UL) or (serverInput.find(S_ERR) == 0UL))
			this->interface->handleResponse(serverInput);
		else if (serverInput.find(S_EVT) == 0UL)
			this->interface->handleEvent(serverInput);
		else
			LOG_WARN(LogContext::GAME_CLIENT, std::format("Unknown server input: '{}'", serverInput));

		startMsg += lenMsg + 1UL;
		this->dataSize -= lenMsg + 1UL;
	}
	if (this->dataSize > 0UL)
		::memmove(this->serverData, startMsg, this->dataSize);
}

void Game::forwardCommandToServer(std::vector<struct pollfd>& pollFds)
{
	// if necessary parse/format command
	// LOG_INFO(LogContext::INTERFACE, "Got new command: " + command);		NB and print this debug msg

	LOG_DEBUG(LogContext::GAME_CLIENT, "Piping command to server");

	if (ioUtils::writeNonBlock(pollFds[2].fd, this->commandBuffer, this->commandLength) == -1)
	{
		LOG_WARN(LogContext::GAME_CLIENT, "Client socket busy, trying again later");
		return;
	}

	pollFds[2].events = 0;
	this->commandLength = 0UL;
}
