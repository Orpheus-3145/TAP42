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
		ClientHTTP(void);

		ClientHTTP(ClientHTTP const&) = delete;
		ClientHTTP& operator=(ClientHTTP const&) = delete;
		ClientHTTP(ClientHTTP&) = delete;
		ClientHTTP& operator=(ClientHTTP&) = delete;

		~ClientHTTP(void);

		void	connect(std::string const& host, uint32_t port);
		void	disconnect(void) noexcept;
		void	stopWorker(void) noexcept;
		void	sendRequest(std::string const& content);
		bool	isConnectionOpen(void) const noexcept { return this->connectionAlive.load(); }

	private:
		void run(void);
		void wakeupWorker(void);
		void handleRead(void);
		bool theresDataToWrite(void) noexcept;
		void handleWrite(void);
		void flushPipe(void) noexcept;

		int32_t			socket{-1};
		Address		 	serverAddress{};
		struct addrinfo	filter{};

		std::thread          worker;
		std::atomic<bool>    connectionAlive{false};

		int32_t wakeupFds[2] = {-1, -1};		// pipe for pollwakeup of worker

		std::mutex              sendMutex;
		std::queue<std::string> sendQueue;

		std::string             currentSend;
		size_t                  currentOffset = 0;

		std::mutex              recvMutex;
		// std::condition_variable recvCond;
		std::queue<std::string> recvQueue;
};
