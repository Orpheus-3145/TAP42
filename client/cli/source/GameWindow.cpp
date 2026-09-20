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
	this->tbdTab = std::make_unique<OutputTab>("", 0, -1, this);
}

void GameWindow::draw(int32_t height, int32_t width)
{
	this->height = ((height - 2) % 5) == 0 ? height : ((height - 2) / 5 * 5 + 2);		// has to be multiple of 5
	this->width = (width % 2) == 0 ? (width - 1) : width;								// has to be an odd number

	this->mainFrame->draw(
		this->height,
		this->width,
		0,
		0
	);

	int32_t widthTabs = (this->width - 2) / 2 - 1;
	// left panel
	this->infoTab->draw(
		6,
		widthTabs,
		1,
		2
	);
	this->infoTab->appendContent("Player: <NAME>");
	this->infoTab->appendContent("Data: <CLASS | RACE | ...>");
	this->infoTab->appendContent("Currently in: <LOCATION>");
	this->infoTab->appendContent("<IN GROUP | NOT IN GROUP>");

	this->commandTab->draw(
		this->height - 6 - 2,
		widthTabs,
		7,
		2
	);

	int32_t tmpHeight = this->height - 2;
	int32_t heightEventsTab = tmpHeight * 2 / 5;
	int32_t tbdTab = tmpHeight / 5;
	this->eventsTab->draw(
		heightEventsTab,
		widthTabs,
		1,
		widthTabs + 3
	);

	this->chatTab->draw(
		heightEventsTab,
		widthTabs,
		heightEventsTab + 1,
		widthTabs + 3
	);

	this->tbdTab->draw(
		tbdTab,
		widthTabs,
		heightEventsTab * 2 + 1,
		widthTabs + 3
	);

	this->tbdTab->appendContent("");
	this->tbdTab->appendContent("");
	this->tbdTab->appendContent("TBD ", TextAlign::MID_ALIGN);

	this->currentTab = this->commandTab.get();
	this->currentTab->refresh();
	this->refresh();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing game window size h: {}, w: {}", height, width));
}

void GameWindow::clear(void) noexcept
{
	this->mainFrame.reset();
	this->commandTab.reset();
	this->chatTab.reset();
	this->infoTab.reset();
	this->eventsTab.reset();
	this->tbdTab.reset();
}

void GameWindow::readInput(void)
{
	InputTab* inputTab = dynamic_cast<InputTab*>(this->currentTab);
	assert(inputTab != nullptr and "current input doesn't support handling input");

	inputTab->handleUserInput();
}

void GameWindow::resize(int32_t height, int32_t width)
{
	this->height = ((height - 2) % 5) == 0 ? height : ((height - 2) / 5 * 5 + 2);	// has to be multiple of 5
	this->width = (width % 2) == 0 ? (width - 1) : width;							// has to be an odd number

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

	int32_t widthTabs = (this->width - 2) / 2 - 1;
	// left panel
	this->infoTab->resize(
		6,
		widthTabs,
		1,
		2
	);

	this->commandTab->resize(
		this->height - 6 - 2,
		widthTabs,
		7,
		2
	);

	int32_t tmpHeight = this->height - 2;
	int32_t heightEventsTab = tmpHeight * 2 / 5;
	int32_t tbdTab = tmpHeight / 5;
	this->eventsTab->resize(
		heightEventsTab,
		widthTabs,
		1,
		widthTabs + 3
	);

	this->chatTab->resize(
		heightEventsTab,
		widthTabs,
		heightEventsTab + 1,
		widthTabs + 3
	);

	this->tbdTab->resize(
		tbdTab,
		widthTabs,
		heightEventsTab * 2 + 1,
		widthTabs + 3
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
		this->currentTab = this->chatTab.get();
		this->chatTab->refresh();
	}
	else if (this->currentTab == this->chatTab.get())
	{
		this->currentTab = this->commandTab.get();
		this->commandTab->refresh();
	}
}

void GameWindow::scrollTab(bool goingUp) noexcept
{
	InOutTab* tab = dynamic_cast<InOutTab*>(this->currentTab);
	assert(tab != nullptr and "current tab doesn't support mouse scrolling");

	if (goingUp)
		tab->scrollContentUp();
	else
		tab->scrollContentDown();

	tab->refresh();
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
