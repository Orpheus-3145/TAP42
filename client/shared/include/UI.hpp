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
		UI(int32_t clientSocket) noexcept :
			clientSocket{clientSocket}
		{ assert(clientSocket != -1 and "invalid client socket"); }
	
		UI(UI const& other) = delete;
		UI& operator=(UI const& other) = delete;
		UI(UI&& other) = delete;
		UI& operator=(UI&& other) = delete;

		virtual ~UI(void) noexcept {};

		virtual void startUI(void) = 0;
		virtual void stopUI(void) noexcept = 0;
		virtual void resize(int32_t height, int32_t width) = 0;

	protected:
		virtual void handleCommand(std::string const& command) = 0;
		virtual void handleResponse(std::string const& response) noexcept = 0;
		virtual void handleEvent(std::string const& event) noexcept = 0;

		void readDataFromServer(void);
		void handleServerInput(void);
		bool writeDataToServer(std::string const& command);

		int32_t clientSocket;

		size_t	bufferSize{0UL};
		char	serverBuffer[Config::BUFF_SIZE];
};

std::unique_ptr<UI> uiFactory(int32_t clientSocket);