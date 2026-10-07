#include "GameWindow.hpp"
#include "CLI.hpp"
#include "Logger.hpp"
#include "InOutTab.hpp"

#include <format>
#include <cassert>


GameWindow::GameWindow(int32_t commandFd, UI* engine) :
	TapWindow(engine),
	commandFd{commandFd}
{
	assert(this->commandFd != -1 and "Invalid fd provided for forwarding commands");

	this->tabs[GameWindow::FRAME] = std::make_unique<BasicTab>(0, BLUE_COLOR, this);

	this->tabs[GameWindow::CMD] = std::make_unique<InOutTab>(this->commandFd, CMD_HINTS, Config::PROMPT, "User Events", 0, CYAN_COLOR, this);
	this->tabs[GameWindow::CHAT] = std::make_unique<InOutTab>(this->commandFd, CHAT_CMD_HINTS, Config::PROMPT, "Chat", 0, GREEN_COLOR, this);

	this->tabs[GameWindow::INFO] = std::make_unique<OutputTab>(0, BLUE_COLOR, this);
	this->tabs[GameWindow::WORLD] = std::make_unique<OutputTab>("World events", 0, YELLOW_COLOR, this);

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

	OutputTab* tab = dynamic_cast<OutputTab*>(this->tabs.at(GameWindow::INFO).get());
	assert(tab != nullptr and "current tab doesn't support appending content");
	tab->clearContent();
	tab->appendContent("Player: " + this->engine->getUsername(), TextAlign::LEFT, A_NORMAL, false);
	tab->appendContent("Data: <CLASS | RACE | ...>", TextAlign::LEFT, A_NORMAL, false);
	tab->appendContent("Currently in: <LOCATION>", TextAlign::LEFT, A_NORMAL, false);
	tab->appendContent("<IN GROUP | NOT IN GROUP>", TextAlign::LEFT, A_NORMAL, false);

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

void GameWindow::appendResponse(Message const& response, CommandType cmdType)
{
	InOutTab*	tab = nullptr;
	std::string strCommand = toString(cmdType);
	int32_t		colorText = -1;

	// the tab to be updated is the one that sent the command
	if (std::find(CMD_HINTS.begin(), CMD_HINTS.end(), strCommand) != CMD_HINTS.end())
	{
		tab = dynamic_cast<InOutTab*>(this->tabs.at(GameWindow::CMD).get());
		colorText = CYAN_COLOR;
	}
	else if (std::find(CHAT_CMD_HINTS.begin(), CHAT_CMD_HINTS.end(), strCommand) != CHAT_CMD_HINTS.end())
	{
		tab = dynamic_cast<InOutTab*>(this->tabs.at(GameWindow::CHAT).get());
		colorText = GREEN_COLOR;
	}
	else
		assert(false and "command not found in hints");
	assert(tab != nullptr and "current tab doesn't support appending content");

	std::string content = response.toStringResponse(cmdType);
	tab->appendContent(content, TextAlign::LEFT, COLOR_PAIR(colorText) | A_BOLD);
}

void GameWindow::appendEvent(Message const& event)
{
	OutputTab* tab = dynamic_cast<OutputTab*>(this->tabs.at(GameWindow::WORLD).get());
	assert(tab != nullptr and "current tab doesn't support appending content");

	std::string content = event.toStringEvent();
	tab->appendContent(content, TextAlign::LEFT, COLOR_PAIR(YELLOW_COLOR) | A_BOLD);
}

void GameWindow::appendError(Message const& error, CommandType cmdType)
{
	InOutTab* tab = nullptr;
	std::string strCommand = toString(cmdType);

	// the tab to be updated is the one that sent the command
	if (std::find(CMD_HINTS.begin(), CMD_HINTS.end(), strCommand) != CMD_HINTS.end())
		tab = dynamic_cast<InOutTab*>(this->tabs.at(GameWindow::CMD).get());
	else if (std::find(CHAT_CMD_HINTS.begin(), CHAT_CMD_HINTS.end(), strCommand) != CHAT_CMD_HINTS.end())
		tab = dynamic_cast<InOutTab*>(this->tabs.at(GameWindow::CHAT).get());
	else
		assert(false and "command not found in hints");
	assert(tab != nullptr and "current tab doesn't support appending content");

	std::string content = error.toStringError();
	tab->appendContent(content, TextAlign::LEFT, COLOR_PAIR(RED_COLOR) | A_BOLD);
}
