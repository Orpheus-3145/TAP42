#include <cerrno>
#include <cassert>
#include <format>
#include <vector>
#include <cstring>				// strerror, memchr, memeset, memmove

#include "ClientHTTP.hpp"
#include "Exceptions.hpp"
#include "Logger.hpp"


ClientHTTP::ClientHTTP(std::string const& host, uint32_t port, int32_t gameSocket)
{
	assert(gameSocket != -1 and "invalid game socket");

	this->wakeupPipe = ioUtils::createPipe();

	::memset(this->pollFds, 0, ClientHTTP::POLL_SIZE * sizeof(struct pollfd));

	this->pollFds[ClientHTTP::I_PIPE].fd = this->wakeupPipe.out;
	this->pollFds[ClientHTTP::I_GAME].fd = gameSocket;
	this->pollFds[ClientHTTP::I_SERVER].fd = ioUtils::connectToServer(host, port, nullptr);
	
	LOG_INFO(LogContext::HTTP_CLIENT, std::format("Client HTTP connected to host: {} - port: {}", host, port));
	LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Listening to game socket: {}", this->pollFds[ClientHTTP::I_GAME].fd));
	LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Listening to server socket: {}", this->pollFds[ClientHTTP::I_SERVER].fd));
}

ClientHTTP::~ClientHTTP(void)
{
	this->stopWorker();
	ioUtils::closeSocket(this->pollFds[ClientHTTP::I_SERVER].fd);
	ioUtils::closePipe(this->wakeupPipe);

	LOG_DEBUG(LogContext::HTTP_CLIENT, "Client HTTP disconnected");
}

void ClientHTTP::startWorker(void) noexcept
{
	this->worker = std::thread(&ClientHTTP::pollLoop, this);
	this->keepAlive.store(true);

	LOG_INFO(LogContext::HTTP_CLIENT, "Started HTTP_CLIENT worker");
}

void ClientHTTP::stopWorker(void) noexcept
{
	this->exitPoll();

	this->wakeUpWorker();
	if (this->worker.joinable())
	{
		this->worker.join();
		LOG_DEBUG(LogContext::HTTP_CLIENT, "Stopped HTTP_CLIENT worker");
	}
}

void ClientHTTP::wakeUpWorker(void) const noexcept
{
	char byte = 'x';
	ioUtils::write(this->wakeupPipe.in, &byte, 1UL);
}

void ClientHTTP::flushPipe(void) const noexcept
{
	char tmp;
	ioUtils::read(this->wakeupPipe.out, &tmp, sizeof(char));
}

void ClientHTTP::pollLoop(void)
{
	while (this->keepAlive.load())
	{
		this->pollFds[ClientHTTP::I_PIPE].events = POLLIN;
		this->pollFds[ClientHTTP::I_PIPE].revents = 0;
		this->pollFds[ClientHTTP::I_GAME].events = POLLIN;
		this->pollFds[ClientHTTP::I_GAME].revents = 0;
		this->pollFds[ClientHTTP::I_SERVER].events = POLLIN;
		this->pollFds[ClientHTTP::I_SERVER].revents = 0;
		ioUtils::poll(pollFds, ClientHTTP::POLL_SIZE, -1);

		if (pollFds[ClientHTTP::I_PIPE].revents & POLLIN)	// worker awaken from main thread, flush pipe	NB use it to gracelly close the client when user closes session?
			this->flushPipe();
		
		if (pollFds[ClientHTTP::I_GAME].revents & POLLIN)	// request from Game -> send to server
			this->pipeCommandToServer();

		// game closed connection (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (pollFds[ClientHTTP::I_GAME].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_INFO(LogContext::HTTP_CLIENT, "Game socket disconnected, closing session");
			this->exitPoll();
		}

		if (pollFds[ClientHTTP::I_SERVER].revents & POLLIN)	// response or event from server -> send to game
			this->pipeServerInputToGame();

		// server closed connection (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (pollFds[ClientHTTP::I_SERVER].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_WARN(LogContext::HTTP_CLIENT, "Server terminated connection, closing session");
			// NB should keep going and try to reconnect
			this->exitPoll();
		}
	}
}

void ClientHTTP::pipeCommandToServer(void)
{
	int32_t gameSocket = this->pollFds[ClientHTTP::I_GAME].fd;
	int32_t serverSocket = this->pollFds[ClientHTTP::I_SERVER].fd;

	LOG_DEBUG(LogContext::HTTP_CLIENT, "Got command from game");
	if (ioUtils::pipe(gameSocket, serverSocket) == -1L)
	{
		LOG_INFO(LogContext::HTTP_CLIENT, "Game stopped, closing session");
		this->exitPoll();
	}
}

void ClientHTTP::pipeServerInputToGame(void)
{
	int32_t gameSocket = this->pollFds[ClientHTTP::I_GAME].fd;
	int32_t serverSocket = this->pollFds[ClientHTTP::I_SERVER].fd;

	LOG_DEBUG(LogContext::HTTP_CLIENT, "Got response/event from server");
	if (ioUtils::pipe(serverSocket, gameSocket) == -1L)
	{
		LOG_WARN(LogContext::HTTP_CLIENT, "Server terminated connection, closing session");
		// NB should keep going and try to reconnect
		this->exitPoll();
	}
}

