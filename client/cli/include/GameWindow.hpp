#pragma once

#include <ncurses.h>
#include <memory>
#include <cstdint>
#include <vector>

#include "TapWindow.hpp"
#include "Message.hpp"


static std::vector<std::string> CMD_HINTS
{
	"LOOK",
	"MOVE",
	"WHO",
	"TAKE",
	"DROP",
	"INVENTORY",
	"TALK",
	"ATTACK",
	"STATUS",
	"QUEST",
	"QUESTS",
	"QUIT",
	"USE"
};

static std::vector<std::string> CHAT_CMD_HINTS
{
	"GROUP CREATE",
	"GROUP INVITE",
	"GROUP JOIN",
	"GROUP LEAVE",
	"CHAT GROUP",
	"CHAT ROOM",
	"CHAT GLOBAL"
};

class GameWindow : public TapWindow
{
	public:
		using TapWindow::TapWindow;

		GameWindow(int32_t commandFd, UI* engine);

		virtual ~GameWindow(void) noexcept { this->clear(); }

		void draw(int32_t height, int32_t width) override;

		void appendResponse(Message const& response, CommandType cmdType);
		void appendEvent(Message const& event);
		void appendError(Message const& error, CommandType cmdType);

	protected:
		static constexpr size_t FRAME = 0UL;
		static constexpr size_t INFO = 1UL;
		static constexpr size_t WORLD = 2UL;
		static constexpr size_t CMD = 3UL;
		static constexpr size_t CHAT = 4UL;

		int32_t commandFd;
};
