#include <string>
#include <iostream>
#include <cstring>				// memcpy, memset, memmove
#include <unistd.h>				// execve, dup, dup2, pipe, fork, access, close
#include <sys/socket.h>			// socketpair, htons, htonl, ntohs, ntohl, select
#include <netinet/in.h>			// socket, accept, listen, bind, connect
#include <arpa/inet.h>			// htons, htonl, ntohs, ntohl
#include <sys/types.h>			// send, recv
#include <sys/socket.h>			// send, recv
#include <fcntl.h>				// fcntl
#include <cerrno>				// errno

#include "Client.hpp"
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

Client::Client(void) noexcept
{
	this->filter.ai_family = AF_UNSPEC;
	this->filter.ai_protocol = IPPROTO_TCP;
	this->filter.ai_socktype = SOCK_STREAM;
}

Client::Client(Client&& other) noexcept :
	socket(other.socket)
{
	other.socket = -1;
}

Client&	Client::operator=(Client&& other) noexcept
{
	if (this != &other)
	{
		this->socket = other.socket;
		this->serverAddress = other.serverAddress;
		this->filter = other.filter;
		other.socket = -1;
	}
	return *this;
}

Client::~Client(void)
{
	if (this->socket != -1)
	{
		shutdown(this->socket, SHUT_RDWR);
		close(this->socket);
	}
}

void	Client::connect(std::string const& host, uint32_t portNo)
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

	int flags = fcntl(this->socket, F_GETFL, 0);
	if (flags == -1)
		throw(ClientException("Failed to load flags for socket"));
	if (fcntl(this->socket, F_SETFL, flags | O_NONBLOCK) == -1)
		throw(ClientException("Failed to set socket as non-blocking"));

	std::cout << "connected to: " << this->serverAddress.host << " port: " << this->serverAddress.port << "\n";
}

void Client::sendRequest(std::string const& request) const
{
	std::string 		message = request + "\n";
	ssize_t				sendBytes = 0UL, recvBytes = 0UL;

	constexpr size_t	sizeBuf = 1024;
	char 				buf[sizeBuf];
	memset(buf, 0, sizeBuf);

	while (sendBytes < static_cast<ssize_t>(message.size()))
	{
		sendBytes = send(this->socket, message.data() + sendBytes, message.size() - sendBytes, 0);

		// check errors if sendBytes == -1
	}

	do
	{
		recvBytes = recv(this->socket, buf + recvBytes, sizeBuf - recvBytes, 0);

		// recvBytes == 0 means server closed connection
		// responses from server might be chunked: a buffer is necessary 
		if (recvBytes == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			else
				throw ClientException("Reading error from socket: " + std::string(strerror(errno)));
		}
	} while (recvBytes > 0L);
}

