#pragma once

#include <ncurses.h>
#include <memory>
#include <cstdint>
#include <vector>

#include "CurseWindow.hpp"


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
	"QUIT"
};

static std::vector<std::string> CHAT_CMD_HINTS
{
	"GROUP CREATE",
	"GROUP INVITE",
	"GROUP JOIN",
	"GROUP LEAVE",
	"CHAT",
};

class GameWindow : public CurseWindow
{
	public:
		using CurseWindow::CurseWindow;

		GameWindow(int32_t commandFd, int32_t messageFd);

		virtual ~GameWindow(void) noexcept { this->clear(); }

		void draw(int32_t height, int32_t width) override;
		void resize(int32_t height, int32_t width) override;

		void showResponse(std::string const& response);
		void showChatMsg(std::string const& response);
		void showEvent(std::string const& event);

	protected:
		static constexpr size_t FRAME = 0UL;
		static constexpr size_t INFO = 1UL;
		static constexpr size_t CMD = 2UL;
		static constexpr size_t WORLD = 3UL;
		static constexpr size_t CHAT = 4UL;

		int32_t commandFd;
		int32_t messageFd;
};
