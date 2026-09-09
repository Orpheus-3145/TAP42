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
		void exitPoll(void) noexcept { this->keepAlive.store(false);}
		void pipeCommandToServer(void);
		void pipeServerInputToGame(void);

		static constexpr size_t POLL_SIZE = 3UL;
		static constexpr size_t I_PIPE = 0UL;
		static constexpr size_t I_GAME = 1UL;
		static constexpr size_t I_SERVER = 2UL;

		ioUtils::Pipe	wakeupPipe{-1, -1};		// pipe for pollwakeup the worker	
		struct pollfd	pollFds[POLL_SIZE];

		std::thread			worker;
		std::atomic<bool>	keepAlive{false};
};
