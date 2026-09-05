#pragma once

#include <string>
#include <cstdint>

#include "ClientHTTP.hpp"
#include "UI.hpp"


class Game
{
	public:
		Game(void) noexcept;
		
		Game(Game const&) = delete;
		Game& operator=(Game const&) = delete;
		Game(Game&&) = delete;
		Game& operator=(Game&&) = delete;

		~Game(void);

		void	run(std::string const& host, uint32_t port);
		bool	isWorkerRunning(void) const noexcept { return this->keepAlive.load(); }
		void	startWorker(int32_t gameSocket) noexcept;
		void	stopWorker(void) noexcept;

	private:
		void wakeUpWorker(void) noexcept;
		void flushPipe(void) const noexcept;

		void pollLoop(int32_t clientSocket);
		void forwardCommandToServer(int32_t clientSocket);
		void readDataFromServer(int32_t clientSocket);
		void handleServerInput(void);

		ioUtils::Pipe commandPipe;
		ioUtils::Pipe wakeupPipe{-1, -1};		// pipe for pollwakeup the worker

		std::unique_ptr<ClientHTTP> 	clientHTTP;
		std::unique_ptr<CommandLineUI>	interface;		// later on might be a pointer for doing poly stuff

		size_t	serverInputLength{0UL};
		char	serverBuffer[Config::BUFF_SIZE];

		size_t	commandLength{0UL};
		char	commandBuffer[Config::BUFF_SIZE];

		std::thread	worker;

		std::atomic<bool>	keepAlive{false};
};
