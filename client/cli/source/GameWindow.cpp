#include "GameWindow.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <format>
#include <cassert>


GameWindow::GameWindow(int32_t height, int32_t width, int32_t commandFd, int32_t messageFd) :
	CurseWindow(height, width),
	commandFd{commandFd},
	messageFd{messageFd}
{
	assert(this->commandFd != -1 and "Invalid fd provided for forwarding commands");
	assert(this->messageFd != -1 and "Invalid fd provided for forwarding chat messages");

	this->height = ((this->height - 2) % 5) == 0 ? this->height : ((this->height - 2) / 5 * 5 + 2);		// has to be multiple of 5
	this->width = (this->width % 2) == 0 ? (this->width - 1) : this->width;								// has to be an odd number

	::init_pair(GameWindow::INFO_COLOR, COLOR_BLUE, COLOR_BLACK);
	::init_pair(GameWindow::CMD_COLOR, COLOR_RED, COLOR_BLACK);
	::init_pair(GameWindow::EVENTS_COLOR, COLOR_GREEN, COLOR_BLACK);
	::init_pair(GameWindow::CHAT_COLOR, COLOR_YELLOW, COLOR_BLACK);

	this->mainFrame = std::make_unique<BasicTab>(0, GameWindow::INFO_COLOR, this);

	this->cmdFrame = std::make_unique<BasicTab>(0, GameWindow::CMD_COLOR, this);
	this->inputCmdTab = std::make_unique<SingleInputTab>(this->commandFd, CMD_HINTS, Config::PROMPT, 0, GameWindow::CMD_COLOR, this);
	this->outputCmdTab = std::make_unique<OutputTab>("User events", 0, GameWindow::CMD_COLOR, this);

	this->chatFrame = std::make_unique<BasicTab>(0, GameWindow::CHAT_COLOR, this);
	this->inputChatTab = std::make_unique<SingleInputTab>(this->messageFd, CHAT_CMD_HINTS, Config::PROMPT, 0, GameWindow::CHAT_COLOR, this);
	this->outputChatTab = std::make_unique<OutputTab>("Chat", 0, GameWindow::CHAT_COLOR, this);

	this->infoTab = std::make_unique<OutputTab>(0, GameWindow::INFO_COLOR, this);
	this->eventsTab = std::make_unique<OutputTab>("World events", 0, GameWindow::EVENTS_COLOR, this);
	this->tbdTab = std::make_unique<OutputTab>("TBD", 0, -1, this);

	this->draw();
}

void GameWindow::draw(void)
{
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
	this->infoTab->appendContent("in group?");

	this->cmdFrame->draw(
		this->height - 6 - 2,
		widthTabs,
		7,
		2
	);

	this->outputCmdTab->draw(
		this->height - 6 - 7,
		widthTabs - 2,
		8,
		3
	);

	this->inputCmdTab->draw(
		3,
		widthTabs - 2,
		this->height - 5,
		3
	);
	// right panel
	int32_t tmpHeight = this->height - 2;
	int32_t heightEventsTab = tmpHeight * 2 / 5;
	int32_t heightoutputChatTab = tmpHeight * 2 / 5 - 3;
	int32_t tbdTab = tmpHeight / 5;
	this->eventsTab->draw(
		heightEventsTab,
		widthTabs,
		1,
		widthTabs + 3
	);

	this->chatFrame->draw(
		heightEventsTab,
		widthTabs,
		heightEventsTab + 1,
		widthTabs + 3
	);

	this->outputChatTab->draw(
		heightoutputChatTab - 2,
		widthTabs - 2,
		heightEventsTab + 2,
		widthTabs + 4
	);

	this->inputChatTab->draw(
		3,
		widthTabs - 2,
		heightEventsTab + heightoutputChatTab,
		widthTabs + 4
	);

	this->tbdTab->draw(
		tbdTab,
		widthTabs,
		heightEventsTab + heightoutputChatTab + 3 + 1,
		widthTabs + 3
	);

	this->currentTab = this->inputCmdTab.get();
	this->currentTab->refresh();
	this->refresh();

	LOG_DEBUG(LogContext::INTERFACE, std::format("CLI window size h: {}, w: {}", height, width));
}

void GameWindow::clear(void) noexcept
{
	this->mainFrame.reset();
	this->cmdFrame.reset();
	this->chatFrame.reset();
	this->inputCmdTab.reset();
	this->inputChatTab.reset();
	this->infoTab.reset();
	this->outputCmdTab.reset();
	this->eventsTab.reset();
	this->outputChatTab.reset();
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
	if ((this->height < Config::MIN_HEIGHT_CLI) or (this->width < Config::MIN_WIDTH_CLI))
	{
		if (this->height < Config::MIN_HEIGHT_CLI)
			this->height = Config::MIN_HEIGHT_CLI;
		if (this->width < Config::MIN_WIDTH_CLI)
			this->width = Config::MIN_WIDTH_CLI;

		std::cout << std::format("\033[8;{};{}t", this->height, this->width) << std::endl;
		LOG_WARN(LogContext::INTERFACE, std::format("Window too small, forced to h: {}, w: {}", this->height, this->width));
		return;
	}
	::resizeterm(this->height, this->width);

	this->mainFrame->resize(
		this->height,
		this->width,
		0,
		0
	);

	// left panel
	int32_t widthTabs = (this->width - 2) / 2 - 1;
	this->infoTab->resize(
		6,
		widthTabs,
		1,
		2
	);
	this->infoTab->appendContent("Player: <NAME>");
	this->infoTab->appendContent("Data: <CLASS | RACE | ...>");
	this->infoTab->appendContent("Currently in: <LOCATION>");
	this->infoTab->appendContent("in group?");

	this->cmdFrame->draw(
		this->height - 6 - 2,
		widthTabs,
		7,
		2
	);
	this->outputCmdTab->resize(
		this->height - 6 - 7,
		widthTabs - 2,
		8,
		3
	);

	this->inputCmdTab->resize(
		3,
		widthTabs - 2,
		this->height - 5,
		3
	);
	// right panel
	int32_t tmpHeight = this->height - 2;
	int32_t heightEventsTab = tmpHeight * 2 / 5;
	int32_t heightoutputChatTab = tmpHeight * 2 / 5 - 3;
	int32_t tbdTab = tmpHeight / 5;
	this->eventsTab->resize(
		heightEventsTab,
		widthTabs,
		1,
		widthTabs + 3
	);

	this->chatFrame->resize(
		heightEventsTab,
		widthTabs,
		heightEventsTab + 1,
		widthTabs + 3
	);

	this->outputChatTab->resize(
		heightoutputChatTab - 2,
		widthTabs - 2,
		heightEventsTab + 2,
		widthTabs + 4
	);

	this->inputChatTab->resize(
		3,
		widthTabs - 2,
		heightEventsTab + heightoutputChatTab,
		widthTabs + 4
	);

	this->tbdTab->resize(
		tbdTab,
		widthTabs,
		heightEventsTab + heightoutputChatTab + 3 + 1,
		widthTabs + 3
	);

	this->currentTab->refresh();
	this->refresh();

	// because resize is not handled by ncurses there might be some garbage to read, flush it
	::flushinp();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Window resized to h: {}, w: {}", this->height, this->width));
}

void GameWindow::switchNextTab(void) noexcept
{
	if (this->currentTab == this->inputCmdTab.get())
	{
		this->currentTab = this->inputChatTab.get();
		this->inputChatTab->refresh();
	}
	else if (this->currentTab == this->inputChatTab.get())
	{
		this->currentTab = this->inputCmdTab.get();
		this->inputCmdTab->refresh();
	}
}

void GameWindow::handleResponse(std::string const& response) noexcept
{
	this->outputCmdTab->appendContent(response);
	this->inputCmdTab->refresh();
}

void GameWindow::handleEvent(std::string const& event) noexcept
{
	this->eventsTab->appendContent(event);
	this->inputCmdTab->refresh();
}
