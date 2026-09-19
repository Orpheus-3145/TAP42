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

		GameWindow(int32_t height, int32_t width, int32_t commandFd, int32_t messageFd);

		virtual ~GameWindow(void) noexcept { this->clear(); }

		void handleResponse(std::string const& response) noexcept;
		void handleChatMsg(std::string const& response) noexcept;
		void handleEvent(std::string const& event) noexcept;

		void draw(void) override;
		void clear(void) noexcept override;
		void readInput(void) override;
		void resize(int32_t height, int32_t width) override;
		void switchInputTab(void) noexcept override;
		void scrollTab(bool goingUp) noexcept override;

		static constexpr int32_t INFO_COLOR = 1;
		static constexpr int32_t CMD_COLOR = 2;
		static constexpr int32_t EVENTS_COLOR = 3;
		static constexpr int32_t CHAT_COLOR = 4;

	protected:
		int32_t commandFd;
		int32_t messageFd;

		std::unique_ptr<BasicTab>	mainFrame, cmdFrame, chatFrame;

		std::unique_ptr<OutputTab>	infoTab,
									outputCmdTab,
									eventsTab,
									outputChatTab,
									tbdTab;

		std::unique_ptr<SingleInputTab>	inputCmdTab, inputChatTab;
};
