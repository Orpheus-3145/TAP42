#pragma once

#include <string>
#include <cstdint>
#include <netdb.h> 		// gai_strerror, getaddrinfo, freeaddrinfo
#include <fcntl.h>		// fcntl, macros for I/O
#include <unistd.h>		// read, write, open
#include <poll.h>


namespace ioUtils {

using ::read;
using ::write;
using ::close;
using ::poll;

struct Address
{
	std::string 			host;
	uint32_t				port{0U};
	struct sockaddr_storage rawAddress{};
};

Address	getAddress(const struct sockaddr_storage*) noexcept;
int32_t	connectToServer(std::string const& host, uint32_t port, struct addrinfo* filter);

size_t read(int32_t fd, char* buffer, size_t size);
size_t write(int32_t fd, const char* buffer, size_t size);

ssize_t readNonBlock(int32_t fd, char* buffer, size_t size);
ssize_t writeNonBlock(int32_t fd, const char* buffer, size_t size);

ssize_t pipe(int32_t sourceFd, int32_t destFd);

int32_t createSignalRedirectFd(int32_t signal);

struct SocketPair
{
	int32_t first;
	int32_t second;
};

SocketPair createSocketPair(void);
void closeSocket(int32_t& socket) noexcept;
void closePair(SocketPair& socket) noexcept;

struct Pipe
{
	int32_t in;
	int32_t out;
};

Pipe createPipe(void);
void closePipe(Pipe& pipe) noexcept;

};

void printMutated(std::string const& content) noexcept;
std::string createLogPath(const char* logFolder);
std::string escapeNewLine(const char* buffer, size_t size) noexcept;
bool timerElapsed(int32_t intervalSeconds);
