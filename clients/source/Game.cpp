#include "Game.hpp"
#include "Utils.hpp"
#include "Exceptions.hpp"

#include <cstring>				// strerror, memchr, memeset, memmove
#include <cassert>


Game::Game(void) noexcept
{
	this->commandPipe = ioUtils::createPipe();
	this->wakeupPipe = ioUtils::createPipe();

	this->clientHTTP = std::make_unique<ClientHTTP>();
	this->interface = std::make_unique<CommandLineUI>(this->commandPipe);
}

Game::~Game(void)
{
	ioUtils::closePipe(this->commandPipe);
	ioUtils::closePipe(this->wakeupPipe);
}

void Game::run(std::string const& host, uint32_t port)
{
	this->serverInputLength = 0UL;
	this->commandLength = 0UL;

	ioUtils::SocketPair gameClientSockets = ioUtils::createSocketPair();

	this->clientHTTP->connect(host, port);
	this->clientHTTP->startWorker(gameClientSockets.first);
	(void) host;
	(void) port;
	this->interface->setup();
	this->interface->show();

	this->startWorker(gameClientSockets.second);
	if (this->worker.joinable())
	{
		this->worker.join();
		LOG_DEBUG(LogContext::GAME, "Stopped worker");
	}
	this->clientHTTP->stopWorker();

	LOG_DEBUG(LogContext::GAME, "Stopped worker");

	this->clientHTTP->disconnect();
	this->interface->clear();

	LOG_INFO(LogContext::GAME, "Game stopped");

	ioUtils::closePair(gameClientSockets);
}

void Game::startWorker(int32_t clientSocket) noexcept
{
	assert(clientSocket != -1 and "invalid game socket");

	this->worker = std::thread(&Game::pollLoop, this, clientSocket);
	this->keepAlive.store(true);
	LOG_INFO(LogContext::GAME, "Started game worker, listening to UNIX socket: " + std::to_string(clientSocket));
}

void Game::stopWorker(void) noexcept
{
	this->keepAlive.store(false);

	this->wakeUpWorker();
	if (this->worker.joinable())
	{
		this->worker.join();
		LOG_DEBUG(LogContext::GAME, "Stopped game worker");
	}
}

void Game::wakeUpWorker(void) noexcept
{
	char byte = 'x';
	ioUtils::write(this->wakeupPipe.in, &byte, 1UL);
}

void Game::flushPipe(void) const noexcept
{
	char tmp[64];
	ioUtils::read(this->wakeupPipe.out, tmp, 64);
}

void Game::pollLoop(int32_t clientSocket)
{
	assert(clientSocket != -1 and "invalid client socket");

	while (this->keepAlive.load())
	{
		struct pollfd fds[4];
		// user input
		fds[0].fd = STDIN_FILENO;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		// main thread calls
		fds[1].fd = this->wakeupPipe.out;
		fds[1].events = POLLIN;
		fds[1].revents = 0;
		// user input
		fds[2].fd = this->commandPipe.out;
		fds[2].events = POLLIN;
		fds[2].revents = 0;
		// client socket
		fds[3].fd = clientSocket;
		fds[3].events = POLLIN;
		fds[3].revents = 0;

		if (ioUtils::poll(fds, 4, -1) == -1)
		{
			if (errno == EINTR)
				continue;
			LOG_ERROR(LogContext::UI, "Poll failed: " + std::string(strerror(errno)));
			throw CLIException("poll failed: " + std::string(strerror(errno)));
		}

		if (fds[0].revents & POLLIN)
			this->interface->handleUserInput();

		if (fds[1].revents & POLLIN)	// worker awaken from main thread, flush pipe	NB use it to gracelly close the client when user closes session?
			this->flushPipe();
		
		if (fds[2].revents & POLLIN)
			this->forwardCommandToServer(clientSocket);

		if (fds[3].revents & POLLIN)
			this->readDataFromServer(clientSocket);

		// client closed connection (because server did so) (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (fds[3].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_WARN(LogContext::UI, "Client unexpectedly terminated connection, closing session");
			this->keepAlive.store(false);
		}

		this->interface->refresh();
	}
}

void Game::forwardCommandToServer(int32_t clientSocket)
{
	assert(clientSocket != -1 and "invalid client socket");

	LOG_DEBUG(LogContext::UI, "Forwarding command to client");
	this->commandLength = ioUtils::read(this->commandPipe.out, this->commandBuffer, Config::BUFF_SIZE);
	
	// if necessary parse/format command
	
	if (ioUtils::writeNonBlock(clientSocket, this->commandBuffer, this->commandLength) == -1)
	{
		LOG_WARN(LogContext::GAME, "Game-client socket is busy on write side");
		// maybe add id it to polling until it's done?
	}

	// handle graceful termination
	if (this->commandLength >= ::strlen(Config::QUIT) and !::strncmp(this->commandBuffer, Config::QUIT, ::strlen(Config::QUIT)))
		this->keepAlive.store(false);
	this->commandLength = 0UL;
}

void Game::readDataFromServer(int32_t clientSocket)
{
	assert(clientSocket != -1 and "invalid client socket");

	ssize_t n = 0L;
	do	// while loop because buffer could overflow
	{
		n = ioUtils::readNonBlock(clientSocket, this->serverBuffer + this->serverInputLength, Config::BUFF_SIZE - this->serverInputLength);

		if (n > 0L)
		{
			this->serverInputLength += n;
			LOG_DEBUG(LogContext::UI, "Reading game input from client");
			this->handleServerInput();
		}
		else if (n == -1L)		// client closed connection
		{
			LOG_WARN(LogContext::UI, "Client unexpectedly terminated connection, closing session");
			this->keepAlive.store(false);
		}
	} while(n > 0L);
}

void Game::handleServerInput(void)
{
	char *startMsg = this->serverBuffer, *endMsg = nullptr;
	while (true)
	{
		endMsg = reinterpret_cast<char*>(::memchr(startMsg, Config::MSG_TERM, this->serverInputLength));
		if (endMsg == nullptr)
			break;
		
		// NB better input parsing
		ssize_t lenMsg = endMsg - startMsg;
		if (lenMsg < 2)
			throw CLIException("Bad server input: " + std::string(startMsg, lenMsg));

		if (!::strncmp(startMsg, "OK", 2) or !::strncmp(startMsg, "ERR", 3))
		{
			LOG_INFO(LogContext::UI, "Got new response: '" + std::string(startMsg, lenMsg) + "'");
			this->interface->handleResponse(std::string(startMsg, lenMsg));
		}
		else if (!::strncmp(startMsg, "EVT", 3))
		{
			LOG_INFO(LogContext::UI, "Got new event: '" + std::string(startMsg, lenMsg) + "'");
			this->interface->handleEvent(std::string(startMsg, lenMsg));
		}
		else
		{
			LOG_WARN(LogContext::UI, "Unknown command: '" + std::string(startMsg, lenMsg) + "'");
			this->interface->handleEvent("UNKNWON - " + std::string(startMsg, lenMsg));
		}

		startMsg += lenMsg + 1UL;
		this->serverInputLength -= lenMsg + 1UL;
	}
	if (this->serverInputLength > 0UL)
		::memmove(this->serverBuffer, startMsg, this->serverInputLength);
}
