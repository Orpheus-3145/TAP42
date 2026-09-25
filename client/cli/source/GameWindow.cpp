#include "GameWindow.hpp"
#include "CLI.hpp"
#include "Logger.hpp"
#include "InOutTab.hpp"

#include <format>
#include <cassert>


GameWindow::GameWindow(int32_t commandFd) :
	TapWindow(),
	commandFd{commandFd}
{
	assert(this->commandFd != -1 and "Invalid fd provided for forwarding commands");

	this->tabs[GameWindow::FRAME] = std::make_unique<BasicTab>(0, BLUE_COLOR, this);

	this->tabs[GameWindow::CMD] = std::make_unique<InOutTab>(this->commandFd, CMD_HINTS, Config::PROMPT, "User Events", 0, CYAN_COLOR, this);
	this->tabs[GameWindow::CHAT] = std::make_unique<InOutTab>(this->commandFd, CHAT_CMD_HINTS, Config::PROMPT, "Chat", 0, GREEN_COLOR, this);

	this->tabs[GameWindow::INFO] = std::make_unique<OutputTab>(0, BLUE_COLOR, this);
	this->tabs[GameWindow::WORLD] = std::make_unique<OutputTab>("World events", 0, YELLOW_COLOR, this);

	OutputTab* tab = dynamic_cast<OutputTab*>(this->tabs.at(GameWindow::INFO).get());
	assert(tab != nullptr and "current tab doesn't support appending content");
	tab->appendContent("Player: <NAME>");
	tab->appendContent("Data: <CLASS | RACE | ...>");
	tab->appendContent("Currently in: <LOCATION>");
	tab->appendContent("<IN GROUP | NOT IN GROUP>");

	this->tabsToSkip.insert(GameWindow::FRAME);
	this->tabsToSkip.insert(GameWindow::INFO);
	this->switchActiveTab(GameWindow::CMD);
}

void GameWindow::draw(int32_t height, int32_t width)
{
	this->height = (height % 2) == 0 ? height : height - 1;
	this->width = (width % 2) == 0 ? width : width - 1;

	this->tabs.at(GameWindow::FRAME)->draw(
		this->height,
		this->width,
		0,
		0
	);

	int32_t widthTab = (this->width - 2) / 2 - 1;
	// left panel
	this->tabs.at(GameWindow::INFO)->draw(
		6,
		widthTab,
		1,
		2
	);

	this->tabs.at(GameWindow::WORLD)->draw(
		this->height - 6 - 2,
		widthTab,
		7,
		2
	);

	int32_t heightTab = (this->height - 2) / 2;

	this->tabs.at(GameWindow::CMD)->draw(
		heightTab,
		widthTab,
		1,
		widthTab + 3
	);

	this->tabs.at(GameWindow::CHAT)->draw(
		heightTab,
		widthTab,
		heightTab + 1,
		widthTab + 3
	);

	this->updateContentWindow();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing game window size h: {}, w: {}", height, width));
}

void GameWindow::appendResponse(std::string const& response)
{
	InOutTab* tab = dynamic_cast<InOutTab*>(this->tabs.at(GameWindow::CMD).get());
	assert(tab != nullptr and "current tab doesn't support mouse scrolling");

	tab->appendContent(response, true);
}

void GameWindow::appendChatMsg(std::string const& response)
{
	InOutTab* tab = dynamic_cast<InOutTab*>(this->tabs.at(GameWindow::CHAT).get());
	assert(tab != nullptr and "current tab doesn't support mouse scrolling");

	tab->appendContent(response, true);
}

void GameWindow::appendEvent(std::string const& event)
{
	OutputTab* tab = dynamic_cast<OutputTab*>(this->tabs.at(GameWindow::WORLD).get());
	assert(tab != nullptr and "current tab doesn't support mouse scrolling");

	tab->appendContent(event, true);
}
