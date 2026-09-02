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

ClientHTTP::ClientHTTP(void)
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

void ClientHTTP::disconnect(void) noexcept
{
	this->stopWorker();

	if (this->wakeupFds[0] != -1) close(this->wakeupFds[0]);
	if (this->wakeupFds[1] != -1) close(this->wakeupFds[1]);
	this->wakeupFds[0] = this->wakeupFds[1] = -1;

	shutdown(this->socket, SHUT_RDWR);
	close(this->socket);
	this->socket = -1;
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

	this->connectionAlive.store(true);
	this->worker = std::thread(&ClientHTTP::run, this);
}

void ClientHTTP::stopWorker(void) noexcept
{
	this->connectionAlive.store(false);

	this->wakeupWorker();
	if (this->worker.joinable())
		this->worker.join();
}

void ClientHTTP::sendRequest(std::string const& content)
{
	{
		std::lock_guard<std::mutex> lock(this->sendMutex);
		this->sendQueue.push(content + "\n");
	}
	this->wakeupWorker();
}

void ClientHTTP::wakeupWorker(void)
{
	if (this->wakeupFds[1] != -1)
	{
		char byte = 'x';
		write(this->wakeupFds[1], &byte, 1UL);
	}
}

void ClientHTTP::handleRead(void)
{
	constexpr size_t bufSize = 1024;
	char buf[bufSize];
	std::string accumulated;

	while (true)
	{
		ssize_t n = recv(this->socket, buf, bufSize, 0);

		if (n > 0)
		{
			accumulated.append(buf, static_cast<size_t>(n));
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

	if (accumulated.empty() == false)
	{
		std::lock_guard<std::mutex> lock(this->recvMutex);
		this->recvQueue.push(accumulated);
	}
}

bool ClientHTTP::theresDataToWrite(void) noexcept
{
	if (this->currentSend.empty())
	{
		{
			std::lock_guard<std::mutex> lock(this->sendMutex);
			if (this->sendQueue.empty())
				return false;
			this->currentSend = std::move(this->sendQueue.front());
			this->sendQueue.pop();
		}
		this->currentOffset = 0;
	}
	return true;
}

void ClientHTTP::handleWrite(void)
{
	while (true)
	{
		ssize_t n = ::send(this->socket, this->currentSend.data() + this->currentOffset, this->currentSend.size() - this->currentOffset, 0);

		if (n > 0)
		{
			this->currentOffset += static_cast<size_t>(n);
			if (this->currentOffset == this->currentSend.size())
			{
				this->currentSend.clear();
				this->currentOffset = 0;
			}
			continue;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK)
			break; // wait for next pollout
		if (errno == EINTR)
			continue;

		throw ClientException("send failed: " + std::string(strerror(errno)));
	}
}

void ClientHTTP::flushPipe(void) noexcept
{
	char tmp[64];
	while (read(this->wakeupFds[0], tmp, sizeof(tmp)) > 0);
}

void ClientHTTP::run(void)
{
	while (this->connectionAlive.load())
	{
		struct pollfd fds[2];

		fds[0].fd = this->socket;
		fds[0].events = POLLIN;
		if (this->theresDataToWrite())
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
			this->flushPipe();

		if (fds[0].revents & POLLHUP)			// peer closed connection
		{
			this->handleRead();					// in case there's something left to read by the server 
			this->connectionAlive.store(false);
		}

		if (fds[0].revents & (POLLERR | POLLNVAL))		// error, close connection
			this->connectionAlive.store(false);

		if (fds[0].revents & POLLOUT)
			this->handleWrite();

		if (fds[0].revents & POLLIN)
			this->handleRead();
	}
}
