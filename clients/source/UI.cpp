#include "UI.hpp"
#include "Exceptions.hpp"
#include "ClientHTTP.hpp"

#include <string>
#include <cassert>


InputTab::InputTab(int32_t h, int32_t w, int32_t y, int32_t x, int32_t sendCommandFd, int32_t borderChar) :
	sendCommandFd{sendCommandFd}
{
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
	::waddstr(this->main, Config::PROMPT);
	::wmove(this->main, 0, startX);

	if (this->sendCommandFd != -1)		// means that this tab is supposed to receive input (and forward it)
		::keypad(this->main, TRUE);

	::wnoutrefresh(this->border);
	::wnoutrefresh(this->main);

	this->history.emplace_back(0UL, std::array<char, Config::BUFF_SIZE>{});
}

InputTab::InputTab(InputTab&& other) noexcept :
	border{other.border},
	main{other.main},
	sendCommandFd{other.sendCommandFd},
	history{other.history},
	currentCommandIndex{other.currentCommandIndex}
{
	other.border = nullptr;
	other.main = nullptr;

	::wnoutrefresh(this->border);
	::wnoutrefresh(this->main);
}

InputTab& InputTab::operator=(InputTab&& other) noexcept
{
	if (this != &other)
	{
		if (this->main) ::delwin(this->main);
		if (this->border) ::delwin(this->border);

		this->border = other.border;
		this->main = other.main;
		this->sendCommandFd = other.sendCommandFd;
		this->history = other.history;
		this->currentCommandIndex = other.currentCommandIndex;

		other.border = nullptr;
		other.main = nullptr;

		::wnoutrefresh(this->border);
		::wnoutrefresh(this->main);
	}
	return *this;
}

InputTab::~InputTab(void)
{
	if (this->main) ::delwin(this->main);
	if (this->border) ::delwin(this->border);
}

void InputTab::deleteCharForward(void) noexcept
{
	int32_t y, x;
	(void)y;
	getyx(this->main, y, x);

	size_t& bufferSize = this->history.at(this->currentCommandIndex).first;
	char* commandBuffer = this->history.at(this->currentCommandIndex).second.data();

	if (x == this->startX + static_cast<int32_t>(bufferSize))
		return;

	mvwdelch(this->main, y, x);
	::wmove(this->main, y, x);

	x -= this->startX;

	::memmove(commandBuffer + x, commandBuffer + x + 1, static_cast<int32_t>(bufferSize) - x);
	bufferSize--;

	this->refresh();
}

void InputTab::deleteCharBack(void) noexcept
{
	int32_t y, x;
	getyx(this->main, y, x);

	if (x == this->startX)
		return;

	size_t& bufferSize = this->history.at(this->currentCommandIndex).first;
	char* commandBuffer = this->history.at(this->currentCommandIndex).second.data();

	mvwdelch(this->main, y, x - 1);
	::wmove(this->main, y, x - 1);

	x -= this->startX;

	::memmove(commandBuffer + x - 1, commandBuffer + x, static_cast<int32_t>(bufferSize) - x);
	bufferSize--;

	this->refresh();
}

void InputTab::moveCursorLeft(void) const noexcept
{
	int32_t y, x;
	(void)y;

	getyx(this->main, y, x);

	if (x > this->startX)
		::wmove(this->main, y, x - 1);
}

void InputTab::moveCursorRight(void) const noexcept
{
	int32_t y, x;
	(void)y;
	
	getyx(this->main, y, x);

	int32_t bufferSize = static_cast<int32_t>(this->history.at(this->currentCommandIndex).first);
	if (x < this->startX + bufferSize - 1)
		::wmove(this->main, y, x + 1);
}

int32_t InputTab::getCharInput(void) const noexcept
{
	return ::wgetch(this->main);
}

void InputTab::storeCharInput(void)
{
	this->storeCharInput(this->getCharInput());
}

void InputTab::storeCharInput(char input)
{
	size_t& bufferSize = this->history.at(this->currentCommandIndex).first;
	char* commandBuffer = this->history.at(this->currentCommandIndex).second.data();

	int32_t y, x;
	(void)y;
	getyx(this->main, y, x);

	x -= this->startX;

	if (x < static_cast<int32_t>(bufferSize))
		::memmove(commandBuffer + x + 1, commandBuffer + x, static_cast<int32_t>(bufferSize) - x);
	commandBuffer[x] = input;
	bufferSize++;

	if (bufferSize == Config::BUFF_SIZE)
		throw BufferOverflowException("Command buffer overflow");

	mvwaddstr(this->main, y, 0, Config::PROMPT);
	waddnstr(this->main, commandBuffer, bufferSize);
	this->refresh();
}

void InputTab::showPreviousCommand(void) noexcept
{
	if (this->currentCommandIndex == 0UL)
		return;
	this->currentCommandIndex--;

	size_t& bufferSize = this->history.at(this->currentCommandIndex).first;
	char* commandBuffer = this->history.at(this->currentCommandIndex).second.data();

	int32_t y, x;
	(void)x;
	getyx(this->main, y, x);

	::wmove(this->main, y, 0);
	::wclrtoeol(this->main);
	mvwaddstr(this->main, y, 0, Config::PROMPT);
	waddnstr(this->main, commandBuffer, bufferSize);
	this->refresh();
}

void InputTab::showFollowingCommand(void) noexcept
{
	if (this->currentCommandIndex == this->history.size() - 1UL)
		return;
	this->currentCommandIndex++;

	size_t& bufferSize = this->history.at(this->currentCommandIndex).first;
	char* commandBuffer = this->history.at(this->currentCommandIndex).second.data();

	int32_t y, x;
	(void)x;
	getyx(this->main, y, x);

	::wmove(this->main, y, 0);
	::wclrtoeol(this->main);
	mvwaddstr(this->main, y, 0, Config::PROMPT);
	waddnstr(this->main, commandBuffer, bufferSize);
	this->refresh();

}

void InputTab::forwardCommand(void) noexcept
{
	if (this->sendCommandFd == -1)
		return;

	size_t& bufferSize = this->history.at(this->currentCommandIndex).first;
	char* commandBuffer = this->history.at(this->currentCommandIndex).second.data();

	if (bufferSize > 0UL)
	{
		LOG_INFO(LogContext::UI, "Got new command: " + std::string(commandBuffer, bufferSize));
		ioUtils::write(this->sendCommandFd, commandBuffer, bufferSize);

		this->history.emplace_back(0UL, std::array<char, Config::BUFF_SIZE>{});
		this->currentCommandIndex++;
	}

	int32_t y, x;
	getyx(this->main, y, x);

	mvwaddstr(this->main, y + 1, 0, Config::PROMPT);
}


OutputTab::OutputTab(int32_t h, int32_t w, int32_t y, int32_t x, int32_t borderChar)
{
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

	::wnoutrefresh(this->border);
	::wnoutrefresh(this->main);
}

OutputTab::OutputTab(OutputTab&& other) noexcept :
	border{other.border},
	main{other.main},
	content{std::move(other.content)},
	startShowContentIndex{startShowContentIndex}
{
	other.border = nullptr;
	other.main = nullptr;

	::wnoutrefresh(this->border);
	::wnoutrefresh(this->main);
}

OutputTab& OutputTab::operator=(OutputTab&& other) noexcept
{
	if (this != &other)
	{
		if (this->main) ::delwin(this->main);
		if (this->border) ::delwin(this->border);

		this->border = other.border;
		this->main = other.main;
		this->content = std::move(other.content);
		this->startShowContentIndex = startShowContentIndex;

		other.border = nullptr;
		other.main = nullptr;

		::wnoutrefresh(this->border);
		::wnoutrefresh(this->main);
	}
	return *this;
}

OutputTab::~OutputTab(void)
{
	if (this->main) ::delwin(this->main);
	if (this->border) ::delwin(this->border);
}

void OutputTab::appendContent(std::string const& newContent) noexcept
{
	this->content.push_back(newContent);

	int32_t h, w, y, x;
	(void)x;
	(void)w;
	getmaxyx(this->main, h, w);

	if (static_cast<int32_t>(this->content.size()) > h)
	{
		this->startShowContentIndex++;

		for (int32_t i = 0; i < h; i++)
		{
			::wmove(this->main, i, 0);
			::wclrtoeol(this->main);
			::waddstr(this->main, this->content.at(i + this->startShowContentIndex).data());
		}
	}
	else
	{
		getyx(this->main, y, x);
		mvwaddstr(this->main, y, 0, newContent.data());
		::wmove(this->main, y + 1, 0);
	}

	this->refresh();
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
	printf("\033[8;%d;%dt", Config::HEIGHT_WIN, Config::WIDTH_WIN);
	fflush(stdout);

	::initscr();
	::cbreak();
	::noecho();

	this->tabs.resize(CommandLineUI::N_TABS);
	// main
	this->tabs[CommandLineUI::FRAME_TAB] = InputTab();
	// this->tabs[CommandLineUI::FRAME_TAB].appendContent("insert some shit, 'quit' to close");
	// left inpput panel
	this->tabs[CommandLineUI::INPUT_TAB] = InputTab(LINES - 3, (COLS - 2) / 2, 2, 1, this->commandPipe.in);
	// this->tabs[CommandLineUI::INPUT_TAB].appendContent("this is where the input is shown");
	// right inpput panel
	this->tabs[CommandLineUI::OUTPUT_TAB] = InputTab(LINES - 3, (COLS - 2) / 2, 2, (COLS - 2) / 2 + 1, this->commandPipe.in);
	// this->tabs[CommandLineUI::OUTPUT_TAB].appendContent("this is where the output is shown");
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
	(void) response;
	// this->tabs[OUTPUT_TAB].appendContent(response.data());
}

void CommandLineUI::handleEvent(std::string const& event)
{
	(void) event;
	// this->tabs[OUTPUT_TAB].appendContent(event.data());
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
