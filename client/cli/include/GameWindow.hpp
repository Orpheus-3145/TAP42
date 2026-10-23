#pragma once

#include <ncurses.h>
#include <memory>
#include <cstdint>
#include <vector>

#include "NcWindow.hpp"
#include "Message.hpp"


class GameWindow : public NcWindow
{
	public:
		using NcWindow::NcWindow;

		GameWindow(int32_t commandFd, UI* engine);

		virtual ~GameWindow(void) noexcept { this->clear(); }

		void draw(int32_t height, int32_t width) override;

		void appendResponse(Message const& response, CommandType cmdType);
		void appendEvent(Message const& event);
		void appendError(Message const& error, CommandType cmdType);
		void updatePlayerPanel(PlayerInfo const& data);

	protected:
		static constexpr size_t FRAME = 0UL;
		static constexpr size_t INFO = 1UL;
		static constexpr size_t WORLD = 2UL;
		static constexpr size_t CMD = 3UL;
		static constexpr size_t CHAT = 4UL;

		int32_t commandFd;
};
