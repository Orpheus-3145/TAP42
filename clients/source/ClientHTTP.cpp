#include <iostream>
#include <cstring>				// memcpy, memset, memmove
#include <unistd.h>				// execve, dup, dup2, pipe, fork, access, close
#include <sys/socket.h>			// socketpair, htons, htonl, ntohs, ntohl, select
#include <netinet/in.h>			// socket, accept, listen, bind, connect
#include <arpa/inet.h>			// htons, htonl, ntohs, ntohl
#include <sys/types.h>			// send, recv
#include <sys/socket.h>			// send, recv
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cassert>

#include "ClientHTTP.hpp"
#include "Exceptions.hpp"


Address	getAddress(struct sockaddr_storage const& addr) noexcept
{
	Address address{};
	address.rawAddress = addr;

	if (address.rawAddress.ss_family == AF_INET)
	{
		char ipv4[INET_ADDRSTRLEN];
		struct sockaddr_in *addr_v4 = reinterpret_cast<struct sockaddr_in*>(&address.rawAddress);
		inet_ntop(addr_v4->sin_family, &(addr_v4->sin_addr), ipv4, sizeof(ipv4));
		address.host = std::string(ipv4);
		address.port = ntohs(addr_v4->sin_port);
	}
	else if (address.rawAddress.ss_family == AF_INET6)
	{
		char ipv6[INET6_ADDRSTRLEN];
		struct sockaddr_in6 *addr_v6 = reinterpret_cast<struct sockaddr_in6*>(&address.rawAddress);
		inet_ntop(addr_v6->sin6_family, &(addr_v6->sin6_addr), ipv6, sizeof(ipv6));
		address.host = std::string(ipv6);
		address.port = ntohs(addr_v6->sin6_port);
	}
	return (address);
}

ClientHTTP::ClientHTTP(std::mutex& printMutex)
: printMutex{printMutex}
{
	this->filter.ai_family = AF_UNSPEC;
	this->filter.ai_protocol = IPPROTO_TCP;
	this->filter.ai_socktype = SOCK_STREAM;

	if (pipe(this->wakeupFds) == -1)
		throw(ClientException("Failed to create wakeup pipe: " + std::string(strerror(errno))));

	for (int32_t fd : {this->wakeupFds[0], this->wakeupFds[1]})
	{
		int32_t flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}
}

ClientHTTP::~ClientHTTP(void)
{
	this->disconnect();
}

void ClientHTTP::connect(std::string const& host, uint32_t portNo)
{
	struct addrinfo *list, *tmp;
	struct sockaddr_storage rawServerAddress{};
	std::string port = std::to_string(portNo);

	std::cout << "attempt to connect to: " << host << " port: " << port << "\n";

	if (getaddrinfo(host.data(), port.data(), &this->filter, &list) != 0)
		throw(ClientException("Failed to find addresses for " + host + ":" + port));

	for (tmp = list; tmp != nullptr; tmp = tmp->ai_next)
	{
		this->socket = ::socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol);
		if (this->socket == -1)
			continue;
		if (::connect(this->socket, tmp->ai_addr, tmp->ai_addrlen) == 0)
			break;
		shutdown(this->socket, SHUT_RDWR);
		close(this->socket);
	}
	if (tmp == nullptr)
	{
		freeaddrinfo(list);
		throw(ClientException("No available IP host found for port: " + port));
	}
	std::memcpy(&rawServerAddress, tmp->ai_addr, tmp->ai_addrlen);
	freeaddrinfo(list);
	this->serverAddress = getAddress(rawServerAddress);

	int32_t flags = fcntl(this->socket, F_GETFL, 0);
	if (flags == -1)
		throw(ClientException("Failed to load flags for socket"));
	if (fcntl(this->socket, F_SETFL, flags | O_NONBLOCK) == -1)
		throw(ClientException("Failed to set socket as non-blocking"));

	std::cout << "connected to: " << this->serverAddress.host << " port: " << this->serverAddress.port << "\n";
}

void ClientHTTP::disconnect(void) noexcept
{
	if (this->worker)
		this->stopWorker();

	if (this->wakeupFds[0] != -1) close(this->wakeupFds[0]);
	if (this->wakeupFds[1] != -1) close(this->wakeupFds[1]);
	this->wakeupFds[0] = this->wakeupFds[1] = -1;

	shutdown(this->socket, SHUT_RDWR);
	close(this->socket);
	this->socket = -1;
}

void ClientHTTP::startWorker(void) noexcept
{
	this->connectionAlive.store(true);
	this->worker = std::make_unique<std::thread>(&ClientHTTP::run, this);
}

void ClientHTTP::stopWorker(void) noexcept
{
	this->connectionAlive.store(false);

	this->wakeupWorker();
	if (this->worker->joinable())
		this->worker->join();
	this->worker.reset();
}

void ClientHTTP::sendCommandToServer(std::string const& content)
{
	// format and mapping into rfc4242 request
	assert(this->wakeupFds[1] != -1 and "pipe not available");

	write(this->wakeupFds[1], content.data(), content.size());
}

bool ClientHTTP::hasNewResponse(void) noexcept
{
	std::lock_guard<std::mutex> lock(this->mutexResp);
	return this->pendingResp.empty() == false;
}

bool ClientHTTP::hasNewEvent(void) noexcept
{
	std::lock_guard<std::mutex> lock(this->mutexEvents);
	return this->eventQueue.empty() == false;
}

std::string ClientHTTP::consumeResponse(void) noexcept
{
	std::string response;
	{
		std::lock_guard<std::mutex> lock(this->mutexResp);
		response = this->pendingResp;
		this->pendingResp.clear();
	}
	return response;
}

std::string ClientHTTP::popEvent(void) noexcept
{
	std::string event;
	{
		std::lock_guard<std::mutex> lock(this->mutexEvents);
		assert(this->eventQueue.empty() != true and "no event in queue");
		event = std::move(this->eventQueue.front());
		this->eventQueue.pop();
	}
	return event;
}

void ClientHTTP::setResponse(std::string const& resp)
{
	if (this->waitingForResponse.load() == false)
		throw ClientException("Not supposed to receive a response");
	{
		std::lock_guard<std::mutex> lock(this->mutexResp);
		if (this->pendingResp.empty() == false)
			throw ClientException("Response left in queue");
		this->pendingResp = resp;
	}
	this->waitingForResponse.store(false);
}

void ClientHTTP::addEvent(std::string const& event) noexcept
{
	std::lock_guard<std::mutex> lock(this->mutexEvents);
	this->eventQueue.push(event);
}

void ClientHTTP::wakeupWorker(void)
{
	assert(this->wakeupFds[1] != -1 and "pipe not available");

	char byte = 'x';
	write(this->wakeupFds[1], &byte, 1UL);
}

void ClientHTTP::readServerInput(void)
{
	while (true)
	{
		if (this->readOffset >= ClientHTTP::R_BUFF_SIZE)
			throw ClientException("Reading buffer overflow");
	
		ssize_t n = recv(this->socket, this->readingBuffer + this->readOffset, ClientHTTP::R_BUFF_SIZE - this->readOffset, 0);
		
		if (n > 0)
		{
			this->parseInput(n);
			continue; // maybe there's something else to read
		}
		if (n == 0)	// connection closed by peer
		{
			this->connectionAlive.store(false);
			break;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK)  // nothing else to read for now
			break;
		if (errno == EINTR)
			continue;

		throw ClientException("recv failed: " + std::string(strerror(errno)));
	}
}

void ClientHTTP::parseInput(ssize_t charsRead)
{
	char *startMsg = this->readingBuffer, *endMsg = nullptr;
	this->readOffset += charsRead;

	while (true)
	{
		endMsg = reinterpret_cast<char*>(::memchr(startMsg, MSG_TERM, this->readOffset));
		if (endMsg == nullptr)
			break;

		size_t lenMsg = endMsg - startMsg;
		if (::strstr(startMsg, "OK") or ::strstr(startMsg, "ERR"))
			this->setResponse(std::string(startMsg, lenMsg));
		else if (::strstr(startMsg, "EVT"))
			this->addEvent(std::string(startMsg, lenMsg));
		else
			throw ClientException("Server input not existing");

		startMsg += lenMsg + 1UL;
		this->readOffset -= lenMsg + 1UL;
	}

	size_t charsToRemove = startMsg - this->readingBuffer;
	if (charsToRemove > 0UL)
		::memmove(this->readingBuffer, this->readingBuffer + charsToRemove, this->readOffset);
}

void ClientHTTP::readRequestInput(void)
{
	while (true)
	{
		if (this->writeOffset >= ClientHTTP::R_BUFF_SIZE)
			throw ClientException("Reading buffer overflow");
	
		ssize_t n = read(this->wakeupFds[0], this->writingBuffer + this->writeOffset, ClientHTTP::R_BUFF_SIZE - this->writeOffset);
		if (n > 0)
		{
			this->writeOffset += n;
			continue; // maybe there's something else to read
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK)  // nothing else to read for now
			break;
		if (errno == EINTR)
			continue;

		throw ClientException("Read pipe failed: " + std::string(strerror(errno)));
	}
}

void ClientHTTP::writeRequestOutput(void)
{
	size_t localWriteOffset = 0UL;

	while (true)
	{
		ssize_t n = ::send(this->socket, this->writingBuffer + localWriteOffset, this->writeOffset - localWriteOffset, 0);
		if (n > 0)
		{
			localWriteOffset += n;		
			if (localWriteOffset == this->writeOffset)
				break;

			continue;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK)
			break; // wait for next pollout
		if (errno == EINTR)
			continue;

		throw ClientException("Send failed: " + std::string(strerror(errno)));
	}
	this->writeOffset = 0UL;
	this->waitingForResponse.store(true);
}

void ClientHTTP::printAsync(std::string const& content) const noexcept
{
	std::lock_guard<std::mutex> lock(this->printMutex);
	std::cout << content << std::endl;
}

void ClientHTTP::run(void)
{
	this->pendingReq.clear();
	this->pendingResp.clear();
	while (this->eventQueue.empty() == false)
		this->eventQueue.pop();

	this->readOffset = 0UL;
	this->writeOffset = 0UL;
	::memset(this->readingBuffer, 0, ClientHTTP::R_BUFF_SIZE);
	::memset(this->writingBuffer, 0, ClientHTTP::R_BUFF_SIZE);

	while (this->connectionAlive.load())
	{
		struct pollfd fds[2];

		fds[0].fd = this->socket;
		fds[0].events = POLLIN;
		if (this->writeOffset > 0UL)
			fds[0].events |= POLLOUT;
		fds[0].revents = 0;

		fds[1].fd = this->wakeupFds[0];
		fds[1].events = POLLIN;
		fds[1].revents = 0;

		if (poll(fds, 2, -1) == -1)
		{
			if (errno == EINTR)
				continue;

			throw ClientException("poll failed: " + std::string(strerror(errno)));
		}

		// worker awaken from main thread (could be for manual termination 
		// or for writing a new request)
		if (fds[1].revents & POLLIN)
			this->readRequestInput();

		if (fds[0].revents & POLLHUP)			// peer closed connection
			this->connectionAlive.store(false);

		if (fds[0].revents & (POLLERR | POLLNVAL))		// error, close connection
			this->connectionAlive.store(false);

		if (fds[0].revents & POLLOUT)
			this->writeRequestOutput();

		if (fds[0].revents & POLLIN)
			this->readServerInput();
	}
}
