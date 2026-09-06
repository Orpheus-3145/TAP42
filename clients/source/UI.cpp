#include "UI.hpp"
#include "Exceptions.hpp"
#include "ClientHTTP.hpp"

#include <string>
#include <cstring>
#include <cassert>


Tab::Tab(int32_t h, int32_t w, int32_t y, int32_t x, int32_t borderChar, int32_t sendCommandFd) :
	sendCommandFd{sendCommandFd}
{
	this->history.emplace_back(0UL, std::array<char, Config::BUFF_SIZE>{});

	this->border = ::newwin(h, w, y, x);
	if (this->border == nullptr)
	{
		LOG_ERROR(LogContext::UI, "Failed to create window");
		throw CLIException("Failed to create window");
	}
	::box(this->border, borderChar, borderChar);
	this->main = ::newwin(h - 2, w - 2, y + 1, x + 1);
	if (this->main == nullptr)
	{
		LOG_ERROR(LogContext::UI, "Failed to create window");
		throw CLIException("Failed to create window");
	}

	if (this->sendCommandFd != -1)		// means that this tab is supposed to receive input (and forward it)
		::keypad(this->main, TRUE);

	::wnoutrefresh(this->border);
	::wnoutrefresh(this->main);
}

Tab::Tab(Tab&& other) noexcept :
	history{other.history},
	curentCommandIndex{other.curentCommandIndex},
	border{other.border},
	main{other.main},
	sendCommandFd{other.sendCommandFd}
{
	other.border = nullptr;
	other.main = nullptr;

	::wnoutrefresh(this->border);
	::wnoutrefresh(this->main);
}

Tab& Tab::operator=(Tab&& other) noexcept
{
	if (this != &other)
	{
		if (this->main) ::delwin(this->main);
		if (this->border) ::delwin(this->border);

		this->history = other.history;
		this->curentCommandIndex = other.curentCommandIndex;
		this->border = other.border;
		this->main = other.main;
		this->sendCommandFd = other.sendCommandFd;

		other.border = nullptr;
		other.main = nullptr;

		::wnoutrefresh(this->border);
		::wnoutrefresh(this->main);
	}
	return *this;
}

Tab::~Tab(void)
{
	if (this->main) ::delwin(this->main);
	if (this->border) ::delwin(this->border);
}

void Tab::appendContent(const char* content) noexcept
{
	int32_t y, x;
	getyx(this->main, y, x);

	mvwaddstr(this->main, y, 0, content);
	::wmove(this->main, y + 1, 0);
	this->refresh();
}

void Tab::deleteCharForward(void) noexcept
{
	int32_t y, x;
	(void)y;
	getyx(this->main, y, x);

	size_t& bufferSize = this->history.at(this->curentCommandIndex).first;
	char* commandBuffer = this->history.at(this->curentCommandIndex).second.data();

	if (x == static_cast<int32_t>(bufferSize))
		return;

	mvwdelch(this->main, y, x);
	::wmove(this->main, y, x);

	::memmove(commandBuffer + x, commandBuffer + x + 1, static_cast<int32_t>(bufferSize) - x);
	bufferSize--;

	this->refresh();
}

void Tab::deleteCharBack(void) noexcept
{
	int32_t y, x;
	getyx(this->main, y, x);

	if (x == 0)
		return;

	size_t& bufferSize = this->history.at(this->curentCommandIndex).first;
	char* commandBuffer = this->history.at(this->curentCommandIndex).second.data();

	mvwdelch(this->main, y, x - 1);
	::wmove(this->main, y, x - 1);

	::memmove(commandBuffer + x - 1, commandBuffer + x, static_cast<int32_t>(bufferSize) - x);
	bufferSize--;

	this->refresh();
}

void Tab::moveCursorLeft(void) const noexcept
{
	int32_t y, x;
	(void)y;

	getyx(this->main, y, x);
	if (x > 0)
		::wmove(this->main, y, x - 1);
}

void Tab::moveCursorRight(void) const noexcept
{
	int32_t y, x;
	(void)y;
	
	getyx(this->main, y, x);

	size_t const& bufferSize = this->history.at(this->curentCommandIndex).first;
	if (x < static_cast<int32_t>(bufferSize) - 1)
		::wmove(this->main, y, x + 1);
}

int32_t Tab::getCharInput(void) const noexcept
{
	return ::wgetch(this->main);
}

void Tab::storeCharInput(void)
{
	this->storeCharInput(this->getCharInput());
}

void Tab::storeCharInput(char input)
{
	size_t& bufferSize = this->history.at(this->curentCommandIndex).first;
	char* commandBuffer = this->history.at(this->curentCommandIndex).second.data();

	int32_t y, x;
	(void)y;
	getyx(this->main, y, x);

	if (x < static_cast<int32_t>(bufferSize))
		::memmove(commandBuffer + x + 1, commandBuffer + x, static_cast<int32_t>(bufferSize) - x);
	commandBuffer[x] = input;
	bufferSize++;

	if (bufferSize == Config::BUFF_SIZE)
		throw BufferOverflowException("Command buffer overflow");

	mvwaddnstr(this->main, y, 0, commandBuffer, bufferSize);
	::wmove(this->main, y, x + 1);
	this->refresh();
}

void Tab::showPreviousCommand(void) noexcept
{
	if (this->curentCommandIndex == 0UL)
		return;
	this->curentCommandIndex--;

	size_t& bufferSize = this->history.at(this->curentCommandIndex).first;
	char* commandBuffer = this->history.at(this->curentCommandIndex).second.data();

	int32_t y, x;
	(void)x;
	getyx(this->main, y, x);

	::wmove(this->main, y, 0);
	::wclrtoeol(this->main);
	mvwaddnstr(this->main, y, 0, commandBuffer, bufferSize);
	this->refresh();
}

void Tab::showFollowingCommand(void) noexcept
{
	if (this->curentCommandIndex == this->history.size() - 1UL)
		return;
	this->curentCommandIndex++;

	size_t& bufferSize = this->history.at(this->curentCommandIndex).first;
	char* commandBuffer = this->history.at(this->curentCommandIndex).second.data();

	int32_t y, x;
	(void)x;
	getyx(this->main, y, x);

	::wmove(this->main, y, 0);
	::wclrtoeol(this->main);
	mvwaddnstr(this->main, y, 0, commandBuffer, bufferSize);
	this->refresh();

}

void Tab::forwardCommand(void) noexcept
{
	if (this->sendCommandFd == -1)
		return;

	size_t& bufferSize = this->history.at(this->curentCommandIndex).first;
	char* commandBuffer = this->history.at(this->curentCommandIndex).second.data();

	if (bufferSize > 0UL)
	{
		LOG_INFO(LogContext::UI, "Got new command: " + std::string(commandBuffer, bufferSize));
		ioUtils::write(this->sendCommandFd, commandBuffer, bufferSize);

		this->history.emplace_back(0UL, std::array<char, Config::BUFF_SIZE>{});
		this->curentCommandIndex++;
	}

	int32_t y, x;
	getyx(this->main, y, x);
	::wmove(this->main, y + 1, 0);
}

void Tab::refresh(void) const noexcept
{
	::wnoutrefresh(this->main);
}


CommandLineUI::~CommandLineUI(void)
{
	if (this->tabs.empty() == false)
		this->clear();
}

void CommandLineUI::setup(void)
{
	this->currentTabIndex = 0UL;

	// adjust window size
	printf("\033[8;%d;%dt", HEIGHT_WIN, WIDTH_WIN);
	fflush(stdout);

	::initscr();
	::cbreak();
	::noecho();

	this->tabs.resize(CommandLineUI::N_TABS);
	// main
	this->tabs[CommandLineUI::FRAME_TAB] = Tab();
	this->tabs[CommandLineUI::FRAME_TAB].appendContent("insert some shit, 'quit' to close");
	// left inpput panel
	this->tabs[CommandLineUI::INPUT_TAB] = Tab(LINES - 3, (COLS - 2) / 2, 2, 1, 0, this->commandPipe.in);
	this->tabs[CommandLineUI::INPUT_TAB].appendContent("this is where the input is shown");
	// right inpput panel
	this->tabs[CommandLineUI::OUTPUT_TAB] = Tab(LINES - 3, (COLS - 2) / 2, 2, (COLS - 2) / 2 + 1, 0, this->commandPipe.in);
	this->tabs[CommandLineUI::OUTPUT_TAB].appendContent("this is where the output is shown");
	this->setCurrentTab(CommandLineUI::INPUT_TAB);

	LOG_DEBUG(LogContext::UI, "Setup for CommandLine interface done");
}

void CommandLineUI::clear(void) noexcept
{
	this->tabs.clear();
	::endwin();

	LOG_INFO(LogContext::UI, "UI stopped");
}

void CommandLineUI::handleUserInput(void)
{
	int32_t inputChar = this->tabs[INPUT_TAB].getCharInput();

	switch (inputChar)
	{
		case KEY_RESIZE:	// NB resize doesn't work
			LOG_DEBUG(LogContext::UI, "Resize window callback");
			break;

		case Config::COMMAND_TERM:
			this->tabs[INPUT_TAB].forwardCommand();
			break;

		case KEY_LEFT:
			this->tabs[INPUT_TAB].moveCursorLeft();
			break;
	
		case KEY_RIGHT:
			this->tabs[INPUT_TAB].moveCursorRight();
			break;

		case '\t':
			this->switchForwardTab();
			break;

		case KEY_BTAB:
			this->switchBackwardTab();
			break;
		
		case KEY_DC:
			this->tabs[INPUT_TAB].deleteCharForward();
			break;

		case 127:
		case KEY_BACKSPACE:
		{
			this->tabs[INPUT_TAB].deleteCharBack();
			break;
		}

		case KEY_UP:
			this->tabs[INPUT_TAB].showPreviousCommand();
			break;
			
		case KEY_DOWN:
			this->tabs[INPUT_TAB].showFollowingCommand();
			break;

		default:
			if (inputChar >= 32 && inputChar < 127)
				this->tabs[INPUT_TAB].storeCharInput(inputChar);
			break;
	}
	::doupdate();
}

void CommandLineUI::handleResponse(std::string const& response)
{
	this->tabs[OUTPUT_TAB].appendContent(response.data());
}

void CommandLineUI::handleEvent(std::string const& event)
{
	this->tabs[OUTPUT_TAB].appendContent(event.data());
}

void CommandLineUI::switchForwardTab(void) noexcept
{
	if (this->currentTabIndex < CommandLineUI::N_TABS - 1)
		this->currentTabIndex++;
	else
		this->currentTabIndex = 0UL;
}

void CommandLineUI::switchBackwardTab(void) noexcept
{
	if (this->currentTabIndex > 0UL)
		this->currentTabIndex--;
	else
		this->currentTabIndex = CommandLineUI::N_TABS - 1;
}

void CommandLineUI::setCurrentTab(size_t newTabIndex) noexcept
{
	assert(newTabIndex < CommandLineUI::N_TABS);
	this->currentTabIndex = newTabIndex;
}
