#pragma once

#include <cstdint>
#include <string>
#include <cassert>
#include <memory>

#include "Config.hpp"


static constexpr const char* S_OK = "OK";
static constexpr const char* S_ERR = "ERR";
static constexpr const char* S_EVT = "EVT";
static constexpr const char* QUIT_RESPONSE = "OK bye";

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

		virtual void start(void) = 0;
		virtual void stop(void) noexcept = 0;

	protected:
		void handleInputToServer(void);
		void handleInputFromServer(void);
		
		virtual void handleError(std::string const& errMsg) noexcept = 0;
		virtual void handleServerDisconnect(void) noexcept = 0;

		virtual void handleResponse(std::string const& response) noexcept = 0;
		virtual void handleEvent(std::string const& event) noexcept = 0;

		void splitIntoMessages(void);

		int32_t clientSocket;

		size_t	toServerSize{0UL};
		char	toServerBuffer[Config::BUFF_SIZE];

		size_t	fromServerSize{0UL};
		char	fromServerBuffer[Config::BUFF_SIZE];
};

std::unique_ptr<UI> uiFactory(int32_t clientSocket);