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
	this->width = (this->width % 2) == 0 ? (this->width - 1) : this->width;				// has to be an odd number

	this->frame = std::make_unique<OutputTab>(0, this);
	this->commandTab = std::make_unique<SingleInputTab>(this->commandFd, CMD_HINTS, Config::PROMPT, 0, this);
	this->messageTab = std::make_unique<SingleInputTab>(this->messageFd, CHAT_CMD_HINTS, Config::PROMPT, 0, this);
	this->infoTab = std::make_unique<OutputTab>(0, this);
	this->responseTab = std::make_unique<OutputTab>("Responses", 0, this);
	this->eventTab = std::make_unique<OutputTab>("Events", 0, this);
	this->chatTab = std::make_unique<OutputTab>("Chat", 0, this);
	this->heightTBDTab = std::make_unique<OutputTab>("TBD", 0, this);

	this->show();
}

void GameWindow::show(void)
{
	this->frame->draw(
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

	this->responseTab->draw(
		this->height - 6 - 5,
		widthTabs,
		7,
		2
	);

	this->commandTab->draw(
		3,
		widthTabs,
		this->height - 4,
		2
	);
	// right panel
	int32_t tmpHeight = this->height - 2;
	int32_t heightEventsTab = tmpHeight * 2 / 5;
	int32_t heightChatTab = tmpHeight * 2 / 5 - 3;
	int32_t heightTBDTab = tmpHeight / 5;
	this->eventTab->draw(
		heightEventsTab,
		widthTabs,
		1,
		widthTabs + 3
	);

	this->chatTab->draw(
		heightChatTab,
		widthTabs,
		heightEventsTab + 1,
		widthTabs + 3
	);

	this->messageTab->draw(
		3,
		widthTabs,
		heightEventsTab + heightChatTab + 1,
		widthTabs + 3
	);

	this->heightTBDTab->draw(
		heightTBDTab,
		widthTabs,
		heightEventsTab + heightChatTab + 3 + 1,
		widthTabs + 3
	);

	this->currentTab = this->commandTab.get();
	this->currentTab->refresh();
	this->refresh();

	LOG_DEBUG(LogContext::INTERFACE, std::format("CLI window size h: {}, w: {}", height, width));
}

void GameWindow::clear(void) noexcept
{
	this->frame.reset();
	this->commandTab.reset();
	this->messageTab.reset();
	this->infoTab.reset();
	this->responseTab.reset();
	this->eventTab.reset();
	this->chatTab.reset();
}

void GameWindow::readInput(void)
{
	InputTab* inputTab = dynamic_cast<InputTab*>(this->currentTab);
	assert(inputTab != nullptr and "current input doesn't support handling input");

	inputTab->handleUserInput();
}

void GameWindow::resize(int32_t height, int32_t width)
{
	this->height = ((height - 2) % 5) == 0 ? height : ((height - 2) / 5 * 5 + 2);			// has to be multiple of 5
	this->width = (width % 2) == 0 ? (width - 1) : width;			// has to be an odd number
	::resizeterm(this->height, this->width);

	this->frame->resize(
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
	this->infoTab->appendContent("Player: <NAME>");
	this->infoTab->appendContent("Data: <CLASS | RACE | ...>");
	this->infoTab->appendContent("Currently in: <LOCATION>");
	this->infoTab->appendContent("in group?");

	this->responseTab->resize(
		this->height - 6 - 5,
		widthTabs,
		7,
		2
	);

	this->commandTab->resize(
		3,
		widthTabs,
		this->height - 4,
		2
	);
	// right panel
	int32_t tmpHeight = this->height - 2;
	int32_t heightEventsTab = tmpHeight * 2 / 5;
	int32_t heightChatTab = tmpHeight * 2 / 5;
	int32_t heightTBDTab = tmpHeight - heightEventsTab - heightChatTab;
	this->eventTab->resize(
		heightEventsTab,
		widthTabs,
		1,
		widthTabs + 3
	);

	this->chatTab->resize(
		heightChatTab - 3,
		widthTabs,
		heightEventsTab + 1,
		widthTabs + 3
	);

	this->messageTab->resize(
		3,
		widthTabs,
		heightEventsTab + heightChatTab - 3 + 1,
		widthTabs + 3
	);

	this->heightTBDTab->resize(
		heightTBDTab,
		widthTabs,
		heightEventsTab + heightChatTab + 1,
		widthTabs + 3
	);

	this->currentTab->refresh();
	this->refresh();

	// because resize is not handled by ncurses there might be some garbage to read, flush it
	::flushinp();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Window resized to h: {}, w: {}", height, width));
}

void GameWindow::switchNextTab(void) noexcept
{
	if (this->currentTab == this->commandTab.get())
	{
		this->currentTab = this->messageTab.get();
		this->messageTab->refresh();
	}
	else if (this->currentTab == this->messageTab.get())
	{
		this->currentTab = this->commandTab.get();
		this->commandTab->refresh();
	}
}

void GameWindow::handleResponse(std::string const& response) noexcept
{
	this->responseTab->appendContent(response);
	this->commandTab->refresh();
}

void GameWindow::handleEvent(std::string const& event) noexcept
{
	this->eventTab->appendContent(event);
	this->commandTab->refresh();
}
