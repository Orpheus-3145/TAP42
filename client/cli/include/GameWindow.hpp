#pragma once

#include <ncurses.h>
#include <memory>

#include "CurseWindow.hpp"
#include "CurseTab.hpp"
#include "Config.hpp"


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
		void clear(void) noexcept override;
		void readInput(void) override;
		void resize(int32_t height, int32_t width) override;
		void switchInputTab(void) noexcept override;
		void scrollTab(bool goingUp) noexcept override;

		void showResponse(std::string const& response) noexcept;
		void showChatMsg(std::string const& response) noexcept;
		void showEvent(std::string const& event) noexcept;

	protected:
		int32_t commandFd;
		int32_t messageFd;

		std::unique_ptr<BasicTab>	mainFrame;
		std::unique_ptr<OutputTab>	infoTab, eventsTab;
		std::unique_ptr<InOutTab>	commandTab, chatTab;
};
