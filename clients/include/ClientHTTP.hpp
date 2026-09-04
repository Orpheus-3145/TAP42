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
		ClientHTTP(void);

		ClientHTTP(ClientHTTP const&) = delete;
		ClientHTTP& operator=(ClientHTTP const&) = delete;
		ClientHTTP(ClientHTTP&) = delete;
		ClientHTTP& operator=(ClientHTTP&) = delete;

		~ClientHTTP(void);

		void	connect(std::string const& host, uint32_t port);
		void	disconnect(void) noexcept;
		bool	isConnectionOpen(void) const noexcept { return this->connectionAlive.load(); }
		void	startWorker(int32_t gameSocket) noexcept;
		void	stopWorker(void) noexcept;
	
	private:
		void	wakeUpWorker(void) noexcept;
		void	run(int32_t gameSocket);

		int32_t			gameSocket{-1};
		ioUtils::Pipe	wakeupPipe{-1, -1};		// pipe for pollwakeup the worker	
		int32_t			httpSocket{-1};

		std::thread	worker;

		std::atomic<bool>		connectionAlive{false};
};
