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

	this->pollFds[ClientHTTP::PIPE].fd = this->wakeupPipe.out;
	this->pollFds[ClientHTTP::GAME].fd = gameSocket;
	this->pollFds[ClientHTTP::SERVER].fd = ioUtils::connectToServer(host, port, nullptr);
	
	LOG_INFO(LogContext::HTTP_CLIENT, std::format("Client HTTP connected to host: {} - port: {}", host, port));
	LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Listening to game socket: {}", this->pollFds[ClientHTTP::GAME].fd));
	LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Listening to server socket: {}", this->pollFds[ClientHTTP::SERVER].fd));
}

ClientHTTP::~ClientHTTP(void)
{
	this->stopWorker();
	ioUtils::closeSocket(this->pollFds[ClientHTTP::SERVER].fd);
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
	::memset(this->gameBuffer, 0, Config::BUFF_SIZE);
	::memset(this->serverBuffer, 0, Config::BUFF_SIZE);

	while (this->keepAlive.load())
	{
		this->pollFds[ClientHTTP::PIPE].events = POLLIN;
		this->pollFds[ClientHTTP::PIPE].revents = 0;
		this->pollFds[ClientHTTP::GAME].events |= POLLIN;
		this->pollFds[ClientHTTP::GAME].revents = 0;
		this->pollFds[ClientHTTP::SERVER].events |= POLLIN;
		this->pollFds[ClientHTTP::SERVER].revents = 0;
		ioUtils::poll(pollFds, ClientHTTP::POLL_SIZE, -1);

		if (pollFds[ClientHTTP::PIPE].revents & POLLIN)	// worker awaken from main thread, flush pipe
			this->flushPipe();

		if (pollFds[ClientHTTP::GAME].revents & POLLIN)	// request from Game -> send to server
			this->handleDataFromGame();

		if (pollFds[ClientHTTP::GAME].revents & POLLOUT)	// request from Game -> send to server
			this->handleDataToGame();

		if (pollFds[ClientHTTP::GAME].revents & (POLLHUP | POLLERR | POLLNVAL))
			this->handleGameError();

		if (pollFds[ClientHTTP::SERVER].revents & POLLIN)	// response or event from server -> send to game
			this->handleDataFromServer();

		if (pollFds[ClientHTTP::SERVER].revents & POLLOUT)	// request from Game -> send to server
			this->handleDataToServer();

		if (pollFds[ClientHTTP::SERVER].revents & (POLLHUP | POLLERR | POLLNVAL))
			this->handleServerError();
	}
}

void ClientHTTP::handleDataFromGame(void)
{
	int32_t gameSocket = this->pollFds[ClientHTTP::GAME].fd;

	ssize_t n;
	try
	{
		n = ioUtils::readNonBlock(gameSocket, this->gameBuffer + this->gameBufferSize, Config::BUFF_SIZE - this->gameBufferSize);
	}
	catch(const IOException& e)
	{
		LOG_ERROR(LogContext::HTTP_CLIENT, std::format("I/O error in worker thread: '{}'", e.what()));
		this->exitPoll();
	}

	if (n < 0)
	{
		this->pollFds[ClientHTTP::GAME].revents |= POLLHUP;
		this->handleGameError();
	}
	else if (n > 0)
	{
		this->gameBufferSize += n;
		this->pollFds[ClientHTTP::SERVER].events |= POLLOUT;

		LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Read from game: '{}'", std::string(this->gameBuffer, this->gameBufferSize)));
	}
}

void ClientHTTP::handleDataToGame(void)
{
	int32_t gameSocket = this->pollFds[ClientHTTP::GAME].fd;

	ssize_t n;
	try
	{
		n = ioUtils::writeNonBlock(gameSocket, this->serverBuffer, this->serverBufferSize);
	}
	catch(const IOException& e)
	{
		LOG_ERROR(LogContext::HTTP_CLIENT, std::format("I/O error in worker thread: '{}'", e.what()));
		this->exitPoll();
	}

	if (n > 0)
	{
		LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Sent to game: '{}'", std::string(this->serverBuffer, this->serverBufferSize)));

		this->serverBufferSize -= n;
		if (this->serverBufferSize == 0UL)
			this->pollFds[ClientHTTP::GAME].events = 0;
		else
			::memmove(this->serverBuffer, this->serverBuffer + n, this->serverBufferSize);
	}
}

void ClientHTTP::handleDataFromServer(void)
{
	int32_t serverSocket = this->pollFds[ClientHTTP::SERVER].fd;

	ssize_t n;
	try
	{
		n = ioUtils::readNonBlock(serverSocket, this->serverBuffer + this->serverBufferSize, Config::BUFF_SIZE - this->serverBufferSize);
	}
	catch(const IOException& e)
	{
		LOG_ERROR(LogContext::HTTP_CLIENT, std::format("I/O error in worker thread: '{}'", e.what()));
		this->exitPoll();
	}

	if (n < 0)
	{
		this->pollFds[ClientHTTP::SERVER].revents |= POLLHUP;
		this->handleServerError();
	}
	else if (n > 0)
	{
		this->serverBufferSize += n;
		this->pollFds[ClientHTTP::GAME].events |= POLLOUT;

		LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Read from server: '{}'", std::string(this->serverBuffer, this->serverBufferSize)));
	}

}

void ClientHTTP::handleDataToServer(void)
{
	int32_t serverSocket = this->pollFds[ClientHTTP::SERVER].fd;

	ssize_t n;
	try
	{
		n = ioUtils::writeNonBlock(serverSocket, this->gameBuffer, this->gameBufferSize);
	}
	catch(const IOException& e)
	{
		LOG_ERROR(LogContext::HTTP_CLIENT, std::format("I/O error in worker thread: '{}'", e.what()));
		this->exitPoll();
	}

	if (n > 0)
	{
		LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Sent to server: '{}'", std::string(this->gameBuffer, this->gameBufferSize)));

		this->gameBufferSize -= n;
		if (this->gameBufferSize == 0UL)
			this->pollFds[ClientHTTP::GAME].events = 0;
		else
			::memmove(this->gameBuffer, this->gameBuffer + n, this->gameBufferSize);
	}
}

void ClientHTTP::handleGameError(void) noexcept
{
	if (pollFds[ClientHTTP::GAME].revents & (POLLHUP))		// disconnection
	{
		LOG_WARN(LogContext::HTTP_CLIENT, "Game socket disconnected, closing session");
		this->exitPoll();
	}
	else if (pollFds[ClientHTTP::GAME].revents & (POLLERR))	// invalid socket
	{
		LOG_ERROR(LogContext::HTTP_CLIENT, "Invalid game socket (POLLERR), closing session");
		this->exitPoll();
	}
	else if (pollFds[ClientHTTP::GAME].revents & (POLLNVAL))	// something actually went wrong with poll
	{
		int32_t sockErr = 0;
		socklen_t len = sizeof(sockErr);
	
		if (ioUtils::getsockopt(this->pollFds[ClientHTTP::GAME].fd, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0)
		{
			LOG_ERROR(LogContext::HTTP_CLIENT, "Poll error (POLLNVAL) on game socket but getsockopt failed while getting more data: " + std::string(strerror(errno)));
			this->exitPoll();
		}
		else if (sockErr != 0)
		{
			LOG_ERROR(LogContext::HTTP_CLIENT, "Poll error (POLLNVAL) on game socket: " + std::string(strerror(sockErr)));
			this->exitPoll();
		}
	}
}

void ClientHTTP::handleServerError(void) noexcept
{
	if (pollFds[ClientHTTP::SERVER].revents & (POLLHUP))			// disconnection
	{
		LOG_WARN(LogContext::HTTP_CLIENT, "Server socket disconnected, trying to reconnect");
		// update main thread and try to reconnect for x ms, afterwards: this->exitPoll();
	}
	else if (pollFds[ClientHTTP::SERVER].revents & (POLLERR))		// invalid socket
	{
		LOG_ERROR(LogContext::HTTP_CLIENT, "Invalid server socket (POLLERR), closing session");
		// update main thread
		this->exitPoll();
	}
	else if (pollFds[ClientHTTP::SERVER].revents & (POLLNVAL))	// something actually went wrong with poll
	{
		int32_t sockErr = 0;
		socklen_t len = sizeof(sockErr);
	
		if (ioUtils::getsockopt(this->pollFds[ClientHTTP::GAME].fd, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0)
		{
			LOG_ERROR(LogContext::HTTP_CLIENT, "Poll error (POLLNVAL) on server socket, getsockopt failed while getting more data: " + std::string(strerror(errno)));
			this->exitPoll();
		}
		else if (sockErr != 0)
		{
			LOG_ERROR(LogContext::HTTP_CLIENT, "Poll error (POLLNVAL) on server socket: " + std::string(strerror(sockErr)));

			char buffer[Config::BUFF_SIZE];
			ssize_t n = ioUtils::readNonBlock(this->pollFds[ClientHTTP::SERVER].fd, buffer, Config::BUFF_SIZE);

			if (n != -1)		// server left something left to read, might be more data about error
				LOG_ERROR(LogContext::HTTP_CLIENT, std::format("Last data read: '{}'", std::string(buffer, n)));
			this->exitPoll();
		}
		// update main thread
	}
}
