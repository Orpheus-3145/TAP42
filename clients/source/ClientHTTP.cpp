#include <cerrno>
#include <cassert>
#include <cstring>				// strerror, memchr, memeset, memmove

#include "ClientHTTP.hpp"
#include "Exceptions.hpp"


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
}

void ClientHTTP::disconnect(void) noexcept
{
	this->stopWorker();
	ioUtils::closeSocket(this->httpSocket);
}

void ClientHTTP::startWorker(int32_t gameSocket) noexcept
{
	assert(gameSocket != -1 and "invalid game socket");

	this->worker = std::thread(&ClientHTTP::run, this, gameSocket);
}

void ClientHTTP::stopWorker(void) noexcept
{
	this->connectionAlive.store(false);
	this->wakeUpWorker();
	if (this->worker.joinable())
		this->worker.join();
}

void ClientHTTP::wakeUpWorker(void) noexcept
{
	char byte = 'x';
	ioUtils::write(this->wakeupPipe.in, &byte, 1UL);
}

void ClientHTTP::run(int32_t gameSocket)
{
	char tmp[64];

	this->connectionAlive.store(true);
	while (this->connectionAlive.load())
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
			throw HTTPException("poll failed: " + std::string(strerror(errno)));
		}

		if (fds[0].revents & POLLIN)	// worker awaken from main thread, flush pipe
			ioUtils::read(this->wakeupPipe.out, tmp, 64);
		
		if (fds[1].revents & POLLIN)	// request from Game -> send to server
		{
			if (ioUtils::pipe(gameSocket, this->httpSocket) == -1L)
				this->connectionAlive.store(false);
		}

		// game closed connection (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (fds[1].revents & (POLLHUP | POLLERR | POLLNVAL))
			this->connectionAlive.store(false);

		if (fds[2].revents & POLLIN)	// response or event from server -> send to game
		{
			if (ioUtils::pipe(this->httpSocket, gameSocket) == -1L)
				this->connectionAlive.store(false);
		}

		// server closed connection (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (fds[2].revents & (POLLHUP | POLLERR | POLLNVAL))
			this->connectionAlive.store(false);
	}
	ioUtils::closeSocket(gameSocket);	// close gameSocket so also game gets notified
}
