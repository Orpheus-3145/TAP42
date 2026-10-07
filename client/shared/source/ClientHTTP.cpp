#include <cerrno>
#include <cassert>
#include <format>
#include <vector>
#include <cstring>				// strerror, memchr, memeset, memmove

#include "ClientHTTP.hpp"
#include "UI.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Exceptions.hpp"


ClientHTTP::ClientHTTP(std::string const& host, uint32_t port, int32_t gameSocket) :
	host{host},
	port{port}
{
	assert(gameSocket != -1 and "invalid game socket");

	this->wakeupPipe = ioUtils::createPipe();

	::memset(this->pollFds, 0, ClientHTTP::POLL_SIZE * sizeof(struct pollfd));

	this->pollFds[ClientHTTP::PIPE].fd = this->wakeupPipe.out;
	this->pollFds[ClientHTTP::PIPE].events = POLLIN;
	this->pollFds[ClientHTTP::GAME].fd = gameSocket;
	this->pollFds[ClientHTTP::GAME].events = POLLIN;

	LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Listening to game socket: {}", this->pollFds[ClientHTTP::GAME].fd));
	LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Listening to server socket: {}", this->pollFds[ClientHTTP::SERVER].fd));

	this->connectToServer();
}

ClientHTTP::~ClientHTTP(void)
{
	this->stopWorker();
	ioUtils::closeSocket(this->pollFds[ClientHTTP::SERVER].fd);
	ioUtils::closePipe(this->wakeupPipe);
}

void ClientHTTP::connectToServer(void)
{
	this->pollFds[ClientHTTP::SERVER].fd = ioUtils::connectToServer(this->host, this->port, nullptr);
	this->pollFds[ClientHTTP::SERVER].events = POLLIN;
	if (this->serverBufferSize > 0UL)
		this->pollFds[ClientHTTP::SERVER].events |= POLLOUT;

	LOG_INFO(LogContext::HTTP_CLIENT, std::format("Client HTTP connected to host: {} - port: {}", this->host, this->port));
}

void ClientHTTP::startWorker(void) noexcept
{
	this->worker = std::thread(&ClientHTTP::loop, this);
	this->keepAlive.store(true);

	LOG_INFO(LogContext::HTTP_CLIENT, "Started client worker");
}

void ClientHTTP::stopWorker(void) noexcept
{
	this->exitLoop();
	this->wakeUpWorker();

	if (this->worker.joinable())
	{
		this->worker.join();
		LOG_DEBUG(LogContext::HTTP_CLIENT, "Stopped client worker");
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

void ClientHTTP::loop(void)
{
	// two directions for handling data:
	// 1. from game to server, i.e. sending a command: GAME POLLIN -> SERVER POLLOUT
	// 2. from server to game, i.e. receiving a response or event: SERVER POLLIN -> GAME POLLOUT
	while (this->keepAlive.load())
	{
		try
		{
			this->pollFds[ClientHTTP::PIPE].revents = 0;
			this->pollFds[ClientHTTP::GAME].revents = 0;
			this->pollFds[ClientHTTP::SERVER].revents = 0;
			ioUtils::poll(pollFds, ClientHTTP::POLL_SIZE, -1);
	
			if (pollFds[ClientHTTP::PIPE].revents & POLLIN)		// worker awaken from main thread, flush pipe
			{
				this->flushPipe();
				if (pollFds[ClientHTTP::SERVER].events == 0)
				{
					LOG_DEBUG(LogContext::HTTP_CLIENT, "Attempt to reconnect to server... ");
					this->connectToServer();
				}
			}
	
			if (pollFds[ClientHTTP::GAME].revents & POLLIN)		// got new command -> store in gameBuffer
				this->handleDataFromGame();
	
			if (pollFds[ClientHTTP::GAME].revents & POLLOUT)	// send server data in serverBuffer to game
				this->handleDataToGame();
	
			if (pollFds[ClientHTTP::GAME].revents & (POLLHUP | POLLERR | POLLNVAL))
				this->handleGameError();
	
			if (pollFds[ClientHTTP::SERVER].revents & POLLIN)	// got new server data -> store in serverBuffer
				this->handleDataFromServer();
	
			if (pollFds[ClientHTTP::SERVER].revents & POLLOUT)	// send command in gameBuffer to server
				this->handleDataToServer();
	
			if ((pollFds[ClientHTTP::SERVER].revents & (POLLHUP | POLLERR | POLLNVAL)) and
				(pollFds[ClientHTTP::SERVER].events != 0))		// if .events = 0 server already disconnected so ignore handle error
				this->handleServerError();
		}
		catch (AppException const& e)
		{
			LOG_ERROR(LogContext::HTTP_CLIENT, e.what());
			
			if (e.getError().code != ErrorCode::SERVER_CONN_FAILED)
				this->exitLoop();
		}
	}
}

void ClientHTTP::handleDataFromGame(void)
{
	int32_t gameSocket = this->pollFds[ClientHTTP::GAME].fd;
	ssize_t n = ioUtils::readNonBlock(gameSocket, this->gameBuffer + this->gameBufferSize, Config::BUFF_SIZE - this->gameBufferSize);

	if (n < 0L)
		this->pollFds[ClientHTTP::GAME].revents = POLLHUP;		// game UI disconnected
	else if (n > 0L)
	{
		std::string gameData(this->gameBuffer + this->gameBufferSize, n);
		LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Read from game: '{}'", gameData));

		this->gameBufferSize += n;
		this->pollFds[ClientHTTP::SERVER].events |= POLLOUT;
	}
}

void ClientHTTP::handleDataToGame(void)
{
	int32_t gameSocket = this->pollFds[ClientHTTP::GAME].fd;
	ssize_t n = ioUtils::writeNonBlock(gameSocket, this->serverBuffer, this->serverBufferSize);

	if (n < 0L)
		this->pollFds[ClientHTTP::GAME].revents = POLLHUP;		// game UI disconnected
	else if (n > 0L)
	{
		std::string serverData(this->serverBuffer, n);
		LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Sent to game: '{}'", serverData));
		
		this->serverBufferSize -= n;
		if (this->serverBufferSize == 0UL)
			this->pollFds[ClientHTTP::GAME].events = POLLIN;		// is done writing, no POLLOUT anymore
		else
			::memmove(this->serverBuffer, this->serverBuffer + n, this->serverBufferSize);
	}
	else
		LOG_WARN(LogContext::HTTP_CLIENT, "Game socket buffer is busy, try writing later");
}

void ClientHTTP::handleDataFromServer(void)
{
	int32_t serverSocket = this->pollFds[ClientHTTP::SERVER].fd;
	ssize_t n = ioUtils::readNonBlock(serverSocket, this->serverBuffer + this->serverBufferSize, Config::BUFF_SIZE - this->serverBufferSize);

	if (n < 0L)
		this->pollFds[ClientHTTP::SERVER].revents = POLLHUP;		// server disconnected
	else if (n > 0L)
	{
		std::string serverData(this->serverBuffer + this->serverBufferSize, n);
		LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Read from server: '{}'", serverData));

		this->serverBufferSize += n;
		this->pollFds[ClientHTTP::GAME].events |= POLLOUT;
	}
}

void ClientHTTP::handleDataToServer(void)
{
	int32_t serverSocket = this->pollFds[ClientHTTP::SERVER].fd;
	ssize_t n = ioUtils::writeNonBlock(serverSocket, this->gameBuffer, this->gameBufferSize);

	if (n < 0L)
		this->pollFds[ClientHTTP::SERVER].revents = POLLHUP;		// server disconnected
	else if (n > 0L)
	{
		std::string gameData(this->gameBuffer, n);
		LOG_DEBUG(LogContext::HTTP_CLIENT, std::format("Sent to server: '{}'", gameData));

		this->gameBufferSize -= n;
		if (this->gameBufferSize == 0UL)
			this->pollFds[ClientHTTP::SERVER].events = POLLIN;		// is done writing, no POLLOUT anymore
		else
			::memmove(this->gameBuffer, this->gameBuffer + n, this->gameBufferSize);
	}
	else
		LOG_WARN(LogContext::HTTP_CLIENT, "Server socket buffer is busy, try writing later");
}

void ClientHTTP::handleGameError(void)
{
	if (pollFds[ClientHTTP::GAME].revents & (POLLHUP))			// disconnection
		throw AppException(ErrorCode::CLIENT_DISCONNECTED);
	else if (pollFds[ClientHTTP::GAME].revents & (POLLNVAL))	// invalid socket
		throw AppException(ErrorCode::IO_POLL_FAILED, "invalid socket (POLLNVAL)");
	else if (pollFds[ClientHTTP::GAME].revents & (POLLERR))		// something actually went wrong with poll
	{
		int32_t sockErr = 0;
		socklen_t len = sizeof(sockErr);
	
		if (ioUtils::getsockopt(this->pollFds[ClientHTTP::GAME].fd, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, std::format("getsockopt failed during POLLERR: {}", ::strerror(errno)));
		else if (sockErr != 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, ::strerror(sockErr));
	}
}

void ClientHTTP::handleServerError(void)
{
	if (pollFds[ClientHTTP::SERVER].revents & (POLLHUP))			// disconnection
	{
		this->pollFds[ClientHTTP::SERVER].events = 0;		// stop sending/reading data

		// add an error message to the queue of msg to send to game
		if ((this->serverBufferSize + ::strlen(ERR_SERVER_DISC) + 1) > Config::BUFF_SIZE)
		{
			// if buffer is full, truncate current message to make room for ERR_SERVER_DISC
			this->serverBufferSize = Config::BUFF_SIZE - ::strlen(ERR_SERVER_DISC) - 2;
			this->serverBuffer[this->serverBufferSize++] = Config::COMMAND_TERM;
		}
		::memcpy(this->serverBuffer + this->serverBufferSize, ERR_SERVER_DISC, ::strlen(ERR_SERVER_DISC));
		this->serverBufferSize += ::strlen(ERR_SERVER_DISC);
		this->serverBuffer[this->serverBufferSize++] = Config::COMMAND_TERM;

		this->pollFds[ClientHTTP::GAME].events |= POLLOUT;
		LOG_ERROR(LogContext::HTTP_CLIENT, "Server disconnected, messaging game");
	}
	else if (pollFds[ClientHTTP::SERVER].revents & (POLLNVAL))		// invalid socket
		throw AppException(ErrorCode::IO_POLL_FAILED, "invalid socket (POLLNVAL)");
	else if (pollFds[ClientHTTP::SERVER].revents & (POLLERR))		// something actually went wrong with poll
	{
		int32_t sockErr = 0;
		socklen_t len = sizeof(sockErr);
	
		if (ioUtils::getsockopt(this->pollFds[ClientHTTP::SERVER].fd, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, std::format("getsockopt failed during POLLERR: {}", ::strerror(errno)));
		else if (sockErr != 0)
		{
			char buffer[Config::BUFF_SIZE];
			ssize_t n = ioUtils::readNonBlock(this->pollFds[ClientHTTP::SERVER].fd, buffer, Config::BUFF_SIZE);

			std::string moreErrInfo;
			if (n > 0)		// server left something left to read, might be more data about error
			{
				moreErrInfo = std::format("{}, got last message: {}", ::strerror(sockErr), std::string(buffer, n));
				LOG_ERROR(LogContext::HTTP_CLIENT, std::format("Last data read before POLLERR with server socket: '{}'", std::string(buffer, n)));
			}
			else
				moreErrInfo = ::strerror(sockErr);

			throw AppException(ErrorCode::IO_POLL_FAILED, moreErrInfo);
		}
	}
}
