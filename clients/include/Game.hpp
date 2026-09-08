#pragma once

#include <string>
#include <cstdint>

#include "ClientHTTP.hpp"
#include "UI.hpp"


static constexpr const char* S_OK = "OK";
static constexpr const char* S_ERR = "ERR";
static constexpr const char* S_EVT = "EVT";

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
		void	stopWorker(void) noexcept;

	private:
		void wakeUpWorker(void) noexcept;
		void flushPipe(void) const noexcept;

		void pollLoop(int32_t clientSocket, int32_t commandPipeInput);
		void readCommandFromUI(std::vector<struct pollfd>& pollFds);
		void readDataFromServer(int32_t clientSocket);
		void handleServerInput(void);
		void forwardCommandToServer(std::vector<struct pollfd>& pollFds);

		ioUtils::Pipe wakeupPipe;		// pipe for pollwakeup the worker

		std::unique_ptr<ClientHTTP> 	clientHTTP;
		std::unique_ptr<CommandLineUI>	interface;		// later on might be a pointer for doing poly stuff

		size_t	dataSize{0UL};
		char	serverData[Config::BUFF_SIZE];

		size_t	commandLength{0UL};
		char	commandBuffer[Config::BUFF_SIZE];

		std::thread	worker;

		std::atomic<bool>	keepAlive{false};
};
