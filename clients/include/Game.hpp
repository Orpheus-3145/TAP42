#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <memory>

#include "ClientHTTP.hpp"


static constexpr const char* S_OK = "OK";
static constexpr const char* S_ERR = "ERR";
static constexpr const char* S_EVT = "EVT";

class UI
{
	public:
		UI(int32_t commandFd) noexcept : commandFd{commandFd} {}

		UI(UI const& other) = delete;
		UI& operator=(UI const& other) = delete;
		UI(UI&& other) = delete;
		UI& operator=(UI&& other) = delete;

		virtual ~UI(void) noexcept;

		virtual void loop(void) = 0;
		virtual void handleResponse(std::string const& response) = 0;
		virtual void handleEvent(std::string const& event) = 0;
		virtual void forwardCommandToServer(std::string const& command);
		virtual void stop(void ) noexcept { this->KeepAlive = false; }
		
	protected:
		virtual void resize(int32_t height, int32_t width) = 0;
		virtual void refresh(void) noexcept = 0;

		int32_t commandFd;

		bool KeepAlive{true};
};

class Game
{
	public:
		Game(void) noexcept : wakeupPipe{ioUtils::createPipe()} {}
		
		Game(Game const&) = delete;
		Game& operator=(Game const&) = delete;
		Game(Game&&) = delete;
		Game& operator=(Game&&) = delete;

		~Game(void) noexcept { ioUtils::closePipe(this->wakeupPipe); }

		void	run(std::string const& host, uint32_t port);
		bool	isWorkerRunning(void) const noexcept { return this->keepAlive.load(); }
		void	startWorker(int32_t clientSocket, int32_t commandFd) noexcept;
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

		std::unique_ptr<ClientHTTP> clientHTTP;
		std::unique_ptr<UI>	interface;		// later on might be a pointer for doing poly stuff

		size_t	dataSize{0UL};
		char	serverData[Config::BUFF_SIZE];

		size_t	commandLength{0UL};
		char	commandBuffer[Config::BUFF_SIZE];

		std::thread	worker;

		std::atomic<bool>	keepAlive{false};
};
