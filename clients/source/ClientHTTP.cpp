#include <cerrno>
#include <cassert>
#include <cstring>				// strerror, memchr, memeset, memmove

#include "ClientHTTP.hpp"
#include "Exceptions.hpp"
#include "Logger.hpp"


ClientHTTP::ClientHTTP(void)
{
	this->wakeupPipe = ioUtils::createPipe();
}

ClientHTTP::~ClientHTTP(void)
{
	this->disconnect();
	ioUtils::closePipe(this->wakeupPipe);
}

void ClientHTTP::connect(std::string const& host, uint32_t port)
{
	this->httpSocket = ioUtils::connectToServer(host, port, nullptr);

	LOG_INFO(LogContext::HTTP, "Client HTTP connected to host: " + host + " - port: " + std::to_string(port));
}

void ClientHTTP::disconnect(void) noexcept
{
	if (this->httpSocket == -1)
		return;

	this->stopWorker();
	ioUtils::closeSocket(this->httpSocket);

	LOG_DEBUG(LogContext::HTTP, "Client HTTP disconnected");
}

void ClientHTTP::startWorker(int32_t gameSocket) noexcept
{
	assert(gameSocket != -1 and "invalid game socket");

	this->worker = std::thread(&ClientHTTP::pollLoop, this, gameSocket);
	this->keepAlive.store(true);
	LOG_INFO(LogContext::HTTP, "Started HTTP worker, listening to UNIX socket: " + std::to_string(gameSocket));
}

void ClientHTTP::stopWorker(void) noexcept
{
	this->keepAlive.store(false);

	this->wakeUpWorker();
	if (this->worker.joinable())
	{
		this->worker.join();
		LOG_DEBUG(LogContext::HTTP, "Stopped HTP worker");
	}
}

void ClientHTTP::wakeUpWorker(void) noexcept
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

	while (this->keepAlive.load())
	{
		struct pollfd fds[3];
		// to awake manually the thread
		fds[0].fd = this->wakeupPipe.out;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		// game socket
		fds[1].fd = gameSocket;
		fds[1].events = POLLIN;
		fds[1].revents = 0;
		// server socket
		fds[2].fd = this->httpSocket;
		fds[2].events = POLLIN;
		fds[2].revents = 0;
		if (ioUtils::poll(fds, 3, -1) == -1)
		{
			if (errno == EINTR)
				continue;
			LOG_ERROR(LogContext::HTTP, "Poll failed: " + std::string(strerror(errno)));
			break;
		}

		if (fds[0].revents & POLLIN)	// worker awaken from main thread, flush pipe	NB use it to gracelly close the client when user closes session?
			this->flushPipe();
		
		if (fds[1].revents & POLLIN)	// request from Game -> send to server
		{
			LOG_DEBUG(LogContext::HTTP, "Got command from game");
			if (ioUtils::pipe(gameSocket, this->httpSocket) == -1L)
			{
				LOG_INFO(LogContext::HTTP, "Game stopped, closing session");
				this->keepAlive.store(false);
			}
		}

		// game closed connection (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (fds[1].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_INFO(LogContext::HTTP, "Game stopped, closing session");
			this->keepAlive.store(false);
		}

		if (fds[2].revents & POLLIN)	// response or event from server -> send to game
		{
			LOG_DEBUG(LogContext::HTTP, "Got response/event from server");
			if (ioUtils::pipe(this->httpSocket, gameSocket) == -1L)
			{
				LOG_INFO(LogContext::HTTP, "Server terminated connection, closing session");
				this->keepAlive.store(false);
			}
		}

		// server closed connection (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (fds[2].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_INFO(LogContext::HTTP, "Server terminated connection, closing session");
			this->keepAlive.store(false);
		}
	}
}
