#pragma once

#include <cstdint>
#include <string>

#include "Utils.hpp"


class UI
{
	public:
		UI(int32_t commandFd) noexcept : commandFd{commandFd} {}

		UI(UI const& other) = delete;
		UI& operator=(UI const& other) = delete;
		UI(UI&& other) = delete;
		UI& operator=(UI&& other) = delete;

		virtual ~UI(void) noexcept {};

		virtual void loop(void) = 0;
		virtual void handleResponse(std::string const& response) noexcept = 0;
		virtual void handleEvent(std::string const& event) noexcept = 0;
		virtual void forwardCommandToServer(std::string const& command) const
		{
			ioUtils::write(this->commandFd, command.data(), command.size());
		}
		virtual void stop(void ) noexcept { this->KeepAlive = false; }

	protected:
		virtual void resize(int32_t height, int32_t width) = 0;
		virtual void refresh(void) noexcept = 0;

		int32_t commandFd;

		bool KeepAlive{true};
};
