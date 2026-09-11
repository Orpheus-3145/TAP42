#pragma once

#include <cstdint>
#include <string>
#include <cassert>
#include <vector>
#include <memory>
#include <cassert>

#include "Utils.hpp"
#include "Config.hpp"


static constexpr const char* S_OK = "OK";
static constexpr const char* S_ERR = "ERR";
static constexpr const char* S_EVT = "EVT";
static constexpr const char* G_QUIT = "QUIT";

class UI
{
	public:
		UI(int32_t clientSocket) noexcept { assert(clientSocket != -1 and "invalid client socket"); }
	
		UI(UI const& other) = delete;
		UI& operator=(UI const& other) = delete;
		UI(UI&& other) = delete;
		UI& operator=(UI&& other) = delete;

		virtual ~UI(void) noexcept {};

		virtual void loop(void) = 0;
		virtual void exitLoop(void ) noexcept { this->keepAlive = false; }		// move to CLI.hpp

	protected:
		virtual void handleCommand(void) = 0;
		virtual void handleResponse(std::string const& response) noexcept = 0;
		virtual void handleEvent(std::string const& event) noexcept = 0;
		virtual void resize(int32_t height, int32_t width) = 0;
		virtual void refresh(void) noexcept = 0;		// move to CLI.hpp

		void readDataFromServer(int32_t fd);
		void handleServerInput(void);
		bool forwardDataToServer(int32_t fd, std::string const& command);

		size_t	bufferSize{0UL};
		char	serverBuffer[Config::BUFF_SIZE];

		bool keepAlive{true};		// move to CLI.hpp
};

std::unique_ptr<UI> factoryUI(int32_t clientSocket);