#include <cerrno>
#include <cassert>
#include <format>
#include <vector>
#include <cstring>				// strerror, memchr, memeset, memmove

#include "ClientHTTP.hpp"
#include "Exceptions.hpp"
#include "Logger.hpp"


ClientHTTP::ClientHTTP(std::string const& host, uint32_t port)
{
	this->wakeupPipe = ioUtils::createPipe();
	this->httpSocket = ioUtils::connectToServer(host, port, nullptr);

	LOG_INFO(LogContext::HTTP_CLIENT, std::format("Client HTTP connected to host: {} - port: {}", host, port));
}

ClientHTTP::~ClientHTTP(void)
{
	this->disconnect();
	ioUtils::closePipe(this->wakeupPipe);
}

void ClientHTTP::disconnect(void) noexcept
{
	this->stopWorker();
	ioUtils::closeSocket(this->httpSocket);

	LOG_DEBUG(LogContext::HTTP_CLIENT, "Client HTTP disconnected");
}

void ClientHTTP::startWorker(int32_t gameSocket) noexcept
{
	assert(gameSocket != -1 and "invalid game socket");

	this->worker = std::thread(&ClientHTTP::pollLoop, this, gameSocket);
	this->keepAlive.store(true);
	LOG_INFO(LogContext::HTTP_CLIENT, std::format("Started HTTP_CLIENT worker, listening to UNIX socket: {}", gameSocket));
}

void ClientHTTP::stopWorker(void) noexcept
{
	this->keepAlive.store(false);

	this->wakeUpWorker();
	if (this->worker.joinable())
	{
		this->worker.join();
		LOG_DEBUG(LogContext::HTTP_CLIENT, "Stopped HTP worker");
	}
}

void ClientHTTP::wakeUpWorker(void) const noexcept
{
	char byte = 'x';
	ioUtils::write(this->wakeupPipe.in, &byte, 1UL);
}

void ClientHTTP::flushPipe(void) const noexcept
{
	char tmp[64];
	ioUtils::read(this->wakeupPipe.out, tmp, 64);
}

void ClientHTTP::pollLoop(int32_t gameSocket)
{
	assert(gameSocket != -1 and "invalid game socket");

	std::vector<struct pollfd> pollFds(3);
	// to awake manually the thread
	pollFds[0].fd = this->wakeupPipe.out;
	// game commands
	pollFds[1].fd = gameSocket;
	// read server data
	pollFds[2].fd = this->httpSocket;

	while (this->keepAlive.load())
	{
		pollFds[0].events = POLLIN;
		pollFds[0].revents = 0;
		pollFds[1].events = POLLIN;
		pollFds[1].revents = 0;
		pollFds[2].events = POLLIN;
		pollFds[2].revents = 0;
		ioUtils::poll(pollFds.data(), pollFds.size(), -1);

		if (pollFds[0].revents & POLLIN)	// worker awaken from main thread, flush pipe	NB use it to gracelly close the client when user closes session?
			this->flushPipe();
		
		if (pollFds[1].revents & POLLIN)	// request from Game -> send to server
			pipeCommandToServer(gameSocket);

		// game closed connection (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (pollFds[1].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_INFO(LogContext::HTTP_CLIENT, "Game stopped, closing session");
			this->keepAlive.store(false);
		}

		if (pollFds[2].revents & POLLIN)	// response or event from server -> send to game
			this->pipeServerInputToGame(gameSocket);

		// server closed connection (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (pollFds[2].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_INFO(LogContext::HTTP_CLIENT, "Server terminated connection, closing session");
			this->keepAlive.store(false);
		}
	}
}

void ClientHTTP::pipeCommandToServer(int32_t gameSocket)
{
	LOG_DEBUG(LogContext::HTTP_CLIENT, "Got command from game");
	if (ioUtils::pipe(gameSocket, this->httpSocket) == -1L)
	{
		LOG_INFO(LogContext::HTTP_CLIENT, "Game stopped, closing session");
		this->keepAlive.store(false);
	}
}

void ClientHTTP::pipeServerInputToGame(int32_t gameSocket)
{
	LOG_DEBUG(LogContext::HTTP_CLIENT, "Got response/event from server");
	if (ioUtils::pipe(this->httpSocket, gameSocket) == -1L)
	{
		LOG_INFO(LogContext::HTTP_CLIENT, "Server terminated connection, closing session");
		this->keepAlive.store(false);
	}

}

