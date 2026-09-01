#include <string>
#include <iostream>
#include <cstring>
#include <unistd.h>           // execve, dup, dup2, pipe, fork, access, close
#include <sys/socket.h>       // socketpair, htons, htonl, ntohs, ntohl, select
#include <netinet/in.h>       // socket, accept, listen, bind, connect
#include <arpa/inet.h>        // htons, htonl, ntohs, ntohl
#include <sys/types.h>        // send, recv
#include <sys/socket.h>       // send, recv

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

void	Client::connect(std::string const& host, uint32_t port)
{
	struct addrinfo *list, *tmp;

	const char* hostChar = host.data();
	const char* portChar = std::to_string(port).data();

	if (getaddrinfo(hostChar, portChar, &this->filter, &list) != 0)
		throw(ClientException("Failed to find addresses for " + std::string(hostChar) + ":" + std::string(portChar)));

	for (tmp = list; tmp != nullptr; tmp = tmp->ai_next)
	{
		std::cerr << "check address" << std::endl;
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
		throw(ClientException("No available IP host found for port: " + std::string(portChar)));
	}
	std::cerr << "found valid" << std::endl;
	struct sockaddr_storage rawServerAddress;
	std::memcpy(&rawServerAddress, tmp->ai_addr, tmp->ai_addrlen);
	freeaddrinfo(list);
	std::cerr << "done address" << std::endl;
	this->serverAddress = getAddress(rawServerAddress);
	std::cout << "connected to: " << this->serverAddress.host << " port: " << this->serverAddress.port << "\n";
}

void Client::sendRequest( std::string const& request ) const
{
	ssize_t	sendBytes, recvBytes;
	char 	buf[1024];
	size_t	sizeBuf=1024;

	sendBytes = send(this->socket, request.data(), request.size(), 0);
	if (sendBytes < (ssize_t) strlen(request.data()))
		throw(ClientException("Failed to send message to: " + this->serverAddress.host));
	recvBytes = recv(this->socket, buf, sizeBuf, 0);
	if (recvBytes < 0)
		throw(ClientException("Failed to read message from: " + this->serverAddress.host));
}

