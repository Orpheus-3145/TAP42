#include "GameWindow.hpp"
#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <format>
#include <cassert>


GameWindow::GameWindow(int32_t commandFd, int32_t messageFd) :
	CurseWindow(),
	commandFd{commandFd},
	messageFd{messageFd}
{
	assert(this->commandFd != -1 and "Invalid fd provided for forwarding commands");
	assert(this->messageFd != -1 and "Invalid fd provided for forwarding chat messages");

	this->mainFrame = std::make_unique<BasicTab>(0, BLUE_COLOR, this);

	this->commandTab = std::make_unique<InOutTab>(this->commandFd, CMD_HINTS, Config::PROMPT, "User Events", 0, RED_COLOR, this);
	this->chatTab = std::make_unique<InOutTab>(this->messageFd, CHAT_CMD_HINTS, Config::PROMPT, "Chat", 0, GREEN_COLOR, this);

	this->infoTab = std::make_unique<OutputTab>(0, BLUE_COLOR, this);
	this->eventsTab = std::make_unique<OutputTab>("World events", 0, YELLOW_COLOR, this);
}

void GameWindow::draw(int32_t height, int32_t width)
{
	this->height = (height % 2) == 0 ? height : height - 1;
	this->width = (width % 2) == 0 ? width : width - 1;

	this->mainFrame->draw(
		this->height,
		this->width,
		0,
		0
	);

	int32_t widthTab = (this->width - 2) / 2 - 1;
	// left panel
	this->infoTab->draw(
		6,
		widthTab,
		1,
		2
	);
	this->infoTab->appendContent("Player: <NAME>");
	this->infoTab->appendContent("Data: <CLASS | RACE | ...>");
	this->infoTab->appendContent("Currently in: <LOCATION>");
	this->infoTab->appendContent("<IN GROUP | NOT IN GROUP>");

	this->commandTab->draw(
		this->height - 6 - 2,
		widthTab,
		7,
		2
	);

	int32_t heightTab = (this->height - 2) / 2;

	this->eventsTab->draw(
		heightTab,
		widthTab,
		1,
		widthTab + 3
	);

	this->chatTab->draw(
		heightTab,
		widthTab,
		heightTab + 1,
		widthTab + 3
	);

	this->commandTab->activate();
	this->currentTab = this->commandTab.get();
	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing game window size h: {}, w: {}", height, width));
}

void GameWindow::clear(void) noexcept
{
	this->mainFrame.reset();
	this->commandTab.reset();
	this->chatTab.reset();
	this->infoTab.reset();
	this->eventsTab.reset();
}

void GameWindow::readInput(void)
{
	InputTab* inputTab = dynamic_cast<InputTab*>(this->currentTab);
	assert(inputTab != nullptr and "current input doesn't support handling input");

	inputTab->handleUserInput();
}

void GameWindow::resize(int32_t height, int32_t width)
{
	this->height = (height % 2) == 0 ? height : height - 1;
	this->width = (width % 2) == 0 ? width : width - 1;

	// force minimum size of the window
	// if ((this->height < Config::MIN_HEIGHT_CLI) or (this->width < Config::MIN_WIDTH_CLI))
	// {
	// 	if (this->height < Config::MIN_HEIGHT_CLI)
	// 		this->height = Config::MIN_HEIGHT_CLI;
	// 	if (this->width < Config::MIN_WIDTH_CLI)
	// 		this->width = Config::MIN_WIDTH_CLI;
	//
	// 	std::cout << std::format("\033[8;{};{}t", this->height, this->width) << std::endl;
	// 	LOG_WARN(LogContext::INTERFACE, std::format("Window too small, forced to h: {}, w: {}", this->height, this->width));
	// 	return;
	// }
	::resizeterm(this->height, this->width);

	this->mainFrame->resize(
		this->height,
		this->width,
		0,
		0
	);

	int32_t widthTab = (this->width - 2) / 2 - 1;
	this->infoTab->resize(
		6,
		widthTab,
		1,
		2
	);
	this->commandTab->resize(
		this->height - 6 - 2,
		widthTab,
		7,
		2
	);

	int32_t heightTab = (this->height - 2) / 2;
	this->eventsTab->resize(
		heightTab,
		widthTab,
		1,
		widthTab + 3
	);
	this->chatTab->resize(
		heightTab,
		widthTab,
		heightTab + 1,
		widthTab + 3
	);

	this->currentTab->refresh();
	this->refresh();

	// because resize is not handled by ncurses there might be some garbage to read, flush it
	::flushinp();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Resized game window to h: {}, w: {}", this->height, this->width));
}

void GameWindow::switchInputTab(void) noexcept
{
	if (this->currentTab == this->commandTab.get())
	{
		this->commandTab->deactivate();
		this->chatTab->activate();
		this->currentTab = this->chatTab.get();
	}
	else if (this->currentTab == this->chatTab.get())
	{
		this->chatTab->deactivate();
		this->commandTab->activate();
		this->currentTab = this->commandTab.get();
	}
}

void GameWindow::scrollTab(bool goingUp) noexcept
{
	OutputTab* tab = dynamic_cast<OutputTab*>(this->currentTab);
	assert(tab != nullptr and "current tab doesn't support mouse scrolling");

	if (goingUp)
		tab->scrollContentUp();
	else
		tab->scrollContentDown();
}

void GameWindow::showResponse(std::string const& response) noexcept
{
	this->commandTab->appendContent(response);
	this->currentTab->refresh();
}

void GameWindow::showChatMsg(std::string const& response) noexcept
{
	this->chatTab->appendContent(response);
	this->currentTab->refresh();
}

void GameWindow::showEvent(std::string const& event) noexcept
{
	this->eventsTab->appendContent(event);
	this->currentTab->refresh();
}
