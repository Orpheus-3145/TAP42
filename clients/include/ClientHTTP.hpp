#pragma once

#include <string>
#include <cstdint>

#include <netdb.h> 		// gai_strerror, getaddrinfo, freeaddrinfo
#include <thread>
#include <atomic>
#include <mutex>
// #include <condition_variable>
#include <queue>
#include <functional>


constexpr inline const char MSG_TERM = '\n';
constexpr inline const char SP = ' ';

struct Address
{
	std::string 			host;
	uint32_t				port{0U};
	struct sockaddr_storage rawAddress{};
};

Address	getAddress(const struct sockaddr_storage*) noexcept;

class ClientHTTP
{
	public:
		ClientHTTP(std::mutex& printMutex);

		ClientHTTP(ClientHTTP const&) = delete;
		ClientHTTP& operator=(ClientHTTP const&) = delete;
		ClientHTTP(ClientHTTP&) = delete;
		ClientHTTP& operator=(ClientHTTP&) = delete;

		~ClientHTTP(void);

		void	connect(std::string const& host, uint32_t port);
		void	disconnect(void) noexcept;
		void	startWorker(void) noexcept;
		void	stopWorker(void) noexcept;
		void	sendCommandToServer(std::string const& content);
		bool	isConnectionOpen(void) const noexcept { return this->connectionAlive.load(); }

		bool		hasNewResponse(void) noexcept;
		bool		hasNewEvent(void) noexcept;
		std::string	consumeResponse(void) noexcept;
		std::string	popEvent(void) noexcept;
		
		void printAsync(std::string const& content) const noexcept;

	private:
		static constexpr size_t R_BUFF_SIZE = 1024UL;

		void run(void);
		void setResponse(std::string const& resp);
		void addEvent(std::string const& event) noexcept;
		void wakeupWorker(void);

		void readServerInput(void);
		void parseInput(ssize_t charsRead);
		void readRequestInput(void);
		void writeRequestOutput(void);

		// void flushPipe(void) noexcept;

		int32_t			socket{-1};
		Address		 	serverAddress{};
		struct addrinfo	filter{};

		std::mutex&				printMutex;

		std::unique_ptr<std::thread>	worker;
		std::atomic<bool>		connectionAlive{false};
		std::atomic<bool>		waitingForResponse{false};

		int32_t wakeupFds[2] = {-1, -1};		// pipe for pollwakeup of worker

		size_t					readOffset = 0;
		char					readingBuffer[R_BUFF_SIZE];

		size_t					writeOffset = 0;
		char					writingBuffer[R_BUFF_SIZE];

		std::string				pendingReq;
		std::mutex				mutexReq;

		std::string				pendingResp;
		std::mutex				mutexResp;

		std::queue<std::string>	eventQueue;
		std::mutex				mutexEvents;

};
