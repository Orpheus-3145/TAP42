#include "Utils.hpp"
#include "Config.hpp"
#include "Exceptions.hpp"

#include <iostream>
#include <format>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <mutex>

#include <cstring>
#include <ctime>
#include <cassert>				// strerror, memchr, memeset, memmove
#include <unistd.h>				// execve, dup, dup2, pipe, fork, access, close
#include <sys/socket.h>			// socketpair, htons, htonl, ntohs, ntohl, select
#include <netinet/in.h>			// socket, accept, listen, bind, connect
#include <arpa/inet.h>			// htons, htonl, ntohs, ntohl
#include <sys/types.h>			// send, recv
#include <sys/socket.h>			// send, recv
#include <signal.h>
#include <sys/signalfd.h>


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
		throw HTTPException("Failed to find addresses for " + host + ":" + port);

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
		throw HTTPException("No available IP host found for port: " + port);
	}
	std::memcpy(&rawServerAddress, tmp->ai_addr, tmp->ai_addrlen);
	::freeaddrinfo(list);

	int32_t flags = ::fcntl(socket, F_GETFL, 0);
	if (flags == -1)
		throw HTTPException("Failed to load flags for socket");
	if (::fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1)
		throw HTTPException("Failed to set socket as non-blocking");

	return socket;
}

ssize_t read(int32_t fd, char* buffer, size_t size)
{
	if (size == 0UL)
		return 0L;
	
	size_t	offset = 0UL;
	while (true)
	{
		ssize_t n = ::read(fd, buffer + offset, size - offset);
		if (n > 0L)
		{
			LOG_DEBUG(LogContext::INPUT_OUTPUT, "Read " + std::to_string(n) + " bytes from fd: " + std::to_string(fd));
			LOG_DEBUG(LogContext::INPUT_OUTPUT, "Read: '" + escapeNewLine(buffer + offset, n) + "'");
			offset += n;
		}
		if (n < 0L)
		{
			LOG_ERROR(LogContext::INTERFACE, "Read failed: " + std::string(strerror(errno)));
			throw ReadException("Read failed: " + std::string(strerror(errno)));
		}
		else
			break;
	}
	return offset;
}

ssize_t write(int32_t fd, const char* buffer, size_t size)
{
	if (size == 0UL)
		return 0L;

	size_t	offset = 0UL;
	while (true)
	{
		ssize_t n = ::write(fd, buffer + offset, size - offset);
		if (n > 0L)
		{
			LOG_DEBUG(LogContext::INPUT_OUTPUT, "Write " + std::to_string(n) + " bytes from fd: " + std::to_string(fd));
			LOG_DEBUG(LogContext::INPUT_OUTPUT, "Write: '" + escapeNewLine(buffer + offset, n) + "'");
			offset += n;
		}
		if (n < 0L)
		{
			LOG_ERROR(LogContext::INTERFACE, "Write failed: " + std::string(strerror(errno)));
			throw ReadException("Write failed: " + std::string(strerror(errno)));
		}
		else
			break;
	}
	return offset;
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
			LOG_DEBUG(LogContext::INPUT_OUTPUT, "Read " + std::to_string(n) + " bytes from fd: " + std::to_string(fd));
			LOG_DEBUG(LogContext::INPUT_OUTPUT, "Read: '" + escapeNewLine(buffer + offset, n) + "'");
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

		LOG_ERROR(LogContext::INPUT_OUTPUT, "Recv failed: " + std::string(strerror(errno)));
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
			LOG_DEBUG(LogContext::INPUT_OUTPUT, "Written " + std::to_string(n) + " bytes on fd: " + std::to_string(fd));
			LOG_DEBUG(LogContext::INPUT_OUTPUT, "Written: '" + escapeNewLine(buffer + offset, size - offset) + "'");
			offset += n;
			if (static_cast<size_t>(offset) == size)
				break;
			continue;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK)	// buffer full, wait for next pollout
		{
			LOG_WARN(LogContext::INPUT_OUTPUT, "Destination buffer is full, try later");
			return -1L;
		}
		if (errno == EINTR)
			continue;

		LOG_ERROR(LogContext::INPUT_OUTPUT, "Send failed: " + std::string(strerror(errno)));
		throw WriteException("Send failed: " + std::string(strerror(errno)));
	}
	return offset;
}

ssize_t pipe(int32_t sourceFd, int32_t destFd)
{
	char	inputBuffer[Config::BUFF_SIZE];
	ssize_t	readSize = 0L;

	while (true)
	{
		readSize = readNonBlock(sourceFd, inputBuffer, Config::BUFF_SIZE);
		if (readSize <= 0L)		// if other peer disconnected or there's nothing else to read
			break;
		LOG_DEBUG(LogContext::INPUT_OUTPUT, "Piping input to the other end");
		if (writeNonBlock(destFd, inputBuffer, readSize) == -1L)
		{
			LOG_ERROR(LogContext::INPUT_OUTPUT, "Couldn't write on destination fd, piping failed");
			throw IOException("Couldn't write on destination fd, piping failed");
		}
	}
	return (readSize);
}

int32_t createSignalRedirectFd(int32_t signal)
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, signal);				// create a filter that signal
	sigprocmask(SIG_BLOCK, &mask, NULL);	// and use it to not block it
	
	int32_t fd = ::signalfd(-1, &mask, 0);
	if (fd == -1)
	{
		LOG_ERROR(LogContext::INPUT_OUTPUT, "Failed to creare a file descriptor to redirect: " + std::to_string(signal));
		throw IOException("Failed to creare a file descriptor to redirect: " + std::to_string(signal));
	}

	int32_t flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		throw InterfaceException("Failed to load flags for socket");
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw InterfaceException("Failed to set socket as non-blocking");

	return fd;
}

SocketPair createSocketPair(void)
{
	int sockets[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == -1)
		throw InterfaceException("Error while creating io socket: " + std::string(strerror(errno)));

	for (int32_t fd : {sockets[0], sockets[1]})
	{
		int32_t flags = fcntl(fd, F_GETFL, 0);
		if (flags == -1)
			throw InterfaceException("Failed to load flags for socket");
		if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
			throw InterfaceException("Failed to set socket as non-blocking");
	}
	LOG_DEBUG(LogContext::INPUT_OUTPUT, "Created socket pair: [" + std::to_string(sockets[0]) + " " + std::to_string(sockets[1]) + "]");
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

void closePair(SocketPair& pair) noexcept
{
	closeSocket(pair.first);
	closeSocket(pair.second);
}

Pipe createPipe(void)
{
	int32_t _pipe[2] = {-1, -1};		// pipe for pollwakeup of worker
	if (::pipe(_pipe) == -1)
		throw HTTPException("Failed to create wakeup pipe: " + std::string(strerror(errno)));

	for (int32_t fd : {_pipe[0], _pipe[1]})
	{
		int32_t flags = ::fcntl(fd, F_GETFL, 0);
		if (flags == -1)
			throw HTTPException("Failed to load flags for socket");
		if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
			throw HTTPException("Failed to set socket as non-blocking");
	}
	LOG_DEBUG(LogContext::INPUT_OUTPUT, "Created pipe: [" + std::to_string(_pipe[0]) + " " + std::to_string(_pipe[1]) + "]");
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

};

void printMutated(std::string const& content) noexcept
{
	static std::mutex printMutex;

	std::lock_guard<std::mutex> lock(printMutex);
	std::cout << content << std::endl;
}

std::string createLogPath(const char* logFolder) noexcept
{
	auto now = std::chrono::system_clock::now();
	std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);

	std::tm tmBuf;
	localtime_r(&nowTimeT, &tmBuf);  // versione thread-safe di localtime (POSIX)

	std::ostringstream oss;
	oss << std::put_time(&tmBuf, "%d-%m-%y");  // DD-mm-AA (anno a 2 cifre)

	return std::format("{}/{}_logfile.log", logFolder, oss.str());
}

std::string escapeNewLine(const char* buffer, size_t size) noexcept
{
	assert(buffer != nullptr and "null buffer pointer");
	std::string escaped;

	for (size_t i = 0UL; i < size; i++)
	{
		if (buffer[i] != '\n')
			escaped.push_back(buffer[i]);
		else
		{
			escaped.push_back('\\');
			escaped.push_back('n');
		}
	}
	return escaped;
}

bool timerElapsed(int32_t intervalSeconds)
{
	static auto lastRun = std::chrono::steady_clock::now();

	auto now = std::chrono::steady_clock::now();
	if (now - lastRun >= std::chrono::seconds(intervalSeconds))
	{
		lastRun = now;
		return true;
	}
	return false;
}