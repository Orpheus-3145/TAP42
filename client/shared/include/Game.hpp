#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <memory>

#include "ClientHTTP.hpp"
#include "UI.hpp"


static constexpr const char* S_OK = "OK";
static constexpr const char* S_ERR = "ERR";
static constexpr const char* S_EVT = "EVT";


class Game
{
	public:
		Game(void) noexcept : wakeupPipe{ioUtils::createPipe()} {}
		
		Game(Game const&) = delete;
		Game& operator=(Game const&) = delete;
		Game(Game&&) = delete;
		Game& operator=(Game&&) = delete;

		~Game(void) noexcept { ioUtils::closePipe(this->wakeupPipe); }

		void run(std::string const& host, uint32_t port);
		bool isWorkerRunning(void) const noexcept { return this->keepAlive.load(); }
		void startWorker(int32_t clientSocket, int32_t commandFd) noexcept;
		void stopWorker(void) noexcept;

	private:
		void wakeUpWorker(void) noexcept;
		void flushPipe(void) const noexcept;

		void pollLoop(int32_t clientSocket, int32_t commandPipeInput);
		void readCommandFromUI(std::vector<struct pollfd>& pollFds);
		void readDataFromServer(int32_t clientSocket);
		void handleServerInput(void);
		void forwardCommandToServer(std::vector<struct pollfd>& pollFds);

		ioUtils::Pipe wakeupPipe;		// pipe for pollwakeup worker

		std::unique_ptr<ClientHTTP> clientHTTP;
		std::unique_ptr<UI>	interface;		// later on might be a pointer for doing poly stuff

		size_t	dataSize{0UL};
		char	serverData[Config::BUFF_SIZE];

		size_t	commandLength{0UL};
		char	commandBuffer[Config::CMD_BUFFER_SIZE];

		std::thread	worker;

		std::atomic<bool>	keepAlive{false};
};
