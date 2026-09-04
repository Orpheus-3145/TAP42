#include "Utils.hpp"
#include "Config.hpp"
#include "Exceptions.hpp"

#include <iostream>
#include <cstring>				// strerror, memchr, memeset, memmove
#include <unistd.h>				// execve, dup, dup2, pipe, fork, access, close
#include <sys/socket.h>			// socketpair, htons, htonl, ntohs, ntohl, select
#include <netinet/in.h>			// socket, accept, listen, bind, connect
#include <arpa/inet.h>			// htons, htonl, ntohs, ntohl
#include <sys/types.h>			// send, recv
#include <sys/socket.h>			// send, recv
#include <mutex>


namespace ioUtils {

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

int32_t	connectToServer(std::string const& host, uint32_t portNo, struct addrinfo* filter)
{
	struct addrinfo *list, *tmp, defaultTCPfilter{};
	defaultTCPfilter.ai_family = AF_UNSPEC;
	defaultTCPfilter.ai_protocol = IPPROTO_TCP;
	defaultTCPfilter.ai_socktype = SOCK_STREAM;

	struct sockaddr_storage rawServerAddress{};
	std::string port = std::to_string(portNo);

	int32_t socket = -1;

	if (filter == nullptr)
		filter = &defaultTCPfilter;

	if (::getaddrinfo(host.data(), port.data(), filter, &list) != 0)
		throw(HTTPException("Failed to find addresses for " + host + ":" + port));

	for (tmp = list; tmp != nullptr; tmp = tmp->ai_next)
	{
		socket = ::socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol);
		if (socket == -1)
			continue;
		if (::connect(socket, tmp->ai_addr, tmp->ai_addrlen) == 0)
			break;
		::shutdown(socket, SHUT_RDWR);
		::close(socket);
	}
	if (tmp == nullptr)
	{
		::freeaddrinfo(list);
		throw(HTTPException("No available IP host found for port: " + port));
	}
	std::memcpy(&rawServerAddress, tmp->ai_addr, tmp->ai_addrlen);
	::freeaddrinfo(list);

	int32_t flags = ::fcntl(socket, F_GETFL, 0);
	if (flags == -1)
		throw(HTTPException("Failed to load flags for socket"));
	if (::fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1)
		throw(HTTPException("Failed to set socket as non-blocking"));

	return socket;
}

ssize_t readNonBlock(int32_t fd, char* buffer, size_t size)
{
	if (size == 0UL)
		return 0L;

	ssize_t offset = 0L;
	while (true)
	{
		ssize_t n = ::recv(fd, buffer + offset, size - offset, 0);
		
		if (n > 0)
		{
			offset += n;
			if (static_cast<size_t>(offset) == size)		// overflow
				break;
			continue;
		}
		else if (n == 0)	// connection closed by peer
			return -1L;

		if (errno == EAGAIN || errno == EWOULDBLOCK)  // nothing else to read for now
			break;
		if (errno == EINTR)
			continue;
		throw ReadException("Recv failed: " + std::string(strerror(errno)));
	}
	return offset;
}

ssize_t writeNonBlock(int32_t fd, const char* buffer, size_t size)
{
	if (size == 0UL)
		return 0L;

	ssize_t offset = 0L;
	while (true)
	{
		ssize_t n = ::send(fd, buffer + offset, size - offset, 0);
		
		if (n > 0)
		{
			offset += n;
			if (static_cast<size_t>(offset) == size)
				break;
			continue;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK)	// buffer full, wait for next pollout
			return -1L;
		if (errno == EINTR)
			continue;

		throw WriteException("Send failed: " + std::string(strerror(errno)));
	}
	return offset;
}

ssize_t pipe(int32_t sourceFd, int32_t destFd)
{
	char	inputBuffer[Config::R_BUFF_SIZE];
	ssize_t	readSize = 0L;

	while (true)
	{
		readSize = readNonBlock(sourceFd, inputBuffer, Config::R_BUFF_SIZE);
		if (readSize <= 0L)
			break;
		if (writeNonBlock(destFd, inputBuffer, readSize) == -1L)
			throw IOException("Can't empty pipe input data into output");
	}
	return (readSize);
}

SocketPair createSocketPair(void)
{
	int sockets[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == -1)
		throw CLIException("Error while creating io socket: " + std::string(strerror(errno)));

	for (int32_t fd : {sockets[0], sockets[1]})
	{
		int32_t flags = fcntl(fd, F_GETFL, 0);
		if (flags == -1)
			throw(CLIException("Failed to load flags for socket"));
		if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
			throw(CLIException("Failed to set socket as non-blocking"));
	}
	return SocketPair{sockets[0], sockets[1]};
}

void closeSocket(int32_t& socket) noexcept
{
	if (socket == -1)
		return;
	::shutdown(socket, SHUT_RDWR);
	::close(socket);
	socket = -1;
}

Pipe createPipe(void)
{
	int32_t _pipe[2] = {-1, -1};		// pipe for pollwakeup of worker
	if (::pipe(_pipe) == -1)
		throw(HTTPException("Failed to create wakeup pipe: " + std::string(strerror(errno))));

	for (int32_t fd : {_pipe[0], _pipe[1]})
	{
		int32_t flags = ::fcntl(fd, F_GETFL, 0);
		if (flags == -1)
			throw(HTTPException("Failed to load flags for socket"));
		if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
			throw(HTTPException("Failed to set socket as non-blocking"));
	}
	return Pipe{_pipe[1], _pipe[0]};
}

void closePipe(Pipe& pipe) noexcept
{
	if (pipe.in != -1)
	{
		::close(pipe.in);
		pipe.in = -1;
	}
	if (pipe.out != -1)
	{
		::close(pipe.out);
		pipe.out = -1;
	}
}

void printMutated(std::string const& content) noexcept
{
	static std::mutex printMutex;

	std::lock_guard<std::mutex> lock(printMutex);
	std::cout << content << std::endl;
}

};
