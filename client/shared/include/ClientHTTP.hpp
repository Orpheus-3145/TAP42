#pragma once

#include <string>
#include <cstdint>

#include <thread>
#include <atomic>

#include "Config.hpp"
#include "Utils.hpp"


class ClientHTTP
{
	public:
		ClientHTTP(std::string const& host, uint32_t port, int32_t gameSocket);

		ClientHTTP(ClientHTTP const&) = delete;
		ClientHTTP& operator=(ClientHTTP const&) = delete;
		ClientHTTP(ClientHTTP&) = delete;
		ClientHTTP& operator=(ClientHTTP&) = delete;

		~ClientHTTP(void);

		bool isWorkerRunning(void) const noexcept { return this->keepAlive.load(); }
		void startWorker(void) noexcept;
		void stopWorker(void) noexcept;

	private:
		void wakeUpWorker(void) const noexcept;
		void flushPipe(void) const noexcept;

		void pollLoop(void);
		void handleDataFromGame(void);
		void handleDataToGame(void);
		void handleDataFromServer(void);
		void handleDataToServer(void);
		void handleGameError(void) noexcept;
		void handleServerError(void) noexcept;
		void exitPoll(void) noexcept { this->keepAlive.store(false);}

		static constexpr size_t POLL_SIZE = 3UL;
		static constexpr size_t PIPE = 0UL;
		static constexpr size_t GAME = 1UL;
		static constexpr size_t SERVER = 2UL;

		ioUtils::Pipe	wakeupPipe{-1, -1};		// pipe for pollwakeup the worker	
		struct pollfd	pollFds[POLL_SIZE];

		size_t	serverBufferSize{0UL};
		char	serverBuffer[Config::BUFF_SIZE];

		size_t	gameBufferSize{0UL};
		char	gameBuffer[Config::BUFF_SIZE];

		std::thread			worker;
		std::atomic<bool>	keepAlive{false};
};
