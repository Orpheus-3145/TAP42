#include "UI.hpp"
#include "Exceptions.hpp"
#include "ClientHTTP.hpp"

#include <string>
#include <cassert>


BasicTab::BasicTab(int32_t h, int32_t w, int32_t y, int32_t x, int32_t borderChar)
{
	int32_t th = 0, tw = 0, ty = 0, tx = 0;
	if (borderChar > -1)
	{
		this->border = ::newwin(h, w, y, x);
		if (this->border == nullptr)
		{
			LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
			throw CLIException("Failed to create window");
		}
		::box(this->border, borderChar, borderChar);
		::wnoutrefresh(this->border);

		th = 2;
		tw = 2;
		ty = 1;
		tx = 1;
	}

	this->main = ::newwin(h - th, w - tw, y + ty, x + tx);
	if (this->main == nullptr)
	{
		LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
		throw CLIException("Failed to create window");
	}
	::wnoutrefresh(this->main);
}

BasicTab::BasicTab(BasicTab&& other) noexcept :
	border{other.border},
	main{other.main}
{
	other.border = nullptr;
	other.main = nullptr;

	if (this->border)
		::wnoutrefresh(this->border);
	::wnoutrefresh(this->main);
}

BasicTab& BasicTab::operator=(BasicTab&& other) noexcept
{
	if (this != &other)
	{
		if (this->main) ::delwin(this->main);
		if (this->border) ::delwin(this->border);

		this->border = other.border;
		this->main = other.main;

		other.border = nullptr;
		other.main = nullptr;

		if (this->border)
			::wnoutrefresh(this->border);
		::wnoutrefresh(this->main);
	}
	return *this;
}

BasicTab::~BasicTab(void)
{
	if (this->main) ::delwin(this->main);
	if (this->border) ::delwin(this->border);
}

void BasicTab::appendContent(std::string const& newContent) noexcept
{
	int32_t y, x;
	(void)x;
	getyx(this->main, y, x);

	waddstr(this->main, newContent.data());
	::wmove(this->main, y + 1, 0);

	this->_state.push_back(newContent);
	this->refresh();
}


InputTab::InputTab(int32_t h, int32_t w, int32_t y, int32_t x, int32_t borderChar, int32_t sendCommandFd) :
	BasicTab(h, w, y, x, borderChar),
	sendCommandFd{sendCommandFd}
{
	keypad(this->main, true);

	::waddstr(this->main, Config::PROMPT);

	if (this->sendCommandFd != -1)		// means that this tab is supposed to receive input (and forward it)
		::keypad(this->main, TRUE);

	::wnoutrefresh(this->main);
}

InputTab::InputTab(InputTab&& other) noexcept :
	BasicTab(std::move(other)),
	sendCommandFd{other.sendCommandFd},
	history{std::move(other.history)},
	hints{std::move(other.hints)},
	currentCommandIndex{other.currentCommandIndex},
	currentSuggestedIndex{other.currentSuggestedIndex},
	bufferSize{other.bufferSize},
	tmpBufferSize{other.tmpBufferSize},
	autocompleteMode{other.autocompleteMode}
{
	::memmove(this->commandBuffer, other.commandBuffer, this->bufferSize);
	::memmove(this->tmpCommandBuffer, other.tmpCommandBuffer, this->tmpBufferSize);
}

InputTab& InputTab::operator=(InputTab&& other) noexcept
{
	if (this != &other)
	{
		BasicTab::operator=(std::move(other));

		this->sendCommandFd = other.sendCommandFd;
		this->history = std::move(other.history);
		this->hints = std::move(other.hints);
		this->currentCommandIndex = other.currentCommandIndex;
		this->currentSuggestedIndex = other.currentSuggestedIndex;
		this->bufferSize = other.bufferSize;
		::memmove(this->commandBuffer, other.commandBuffer, this->bufferSize);
		this->tmpBufferSize = other.tmpBufferSize;
		::memmove(this->tmpCommandBuffer, other.tmpCommandBuffer, this->tmpBufferSize);
		this->autocompleteMode = other.autocompleteMode;
	}
	return *this;
}

void InputTab::deleteCharForward(void) noexcept
{
	int32_t y, x;
	(void)y;
	getyx(this->main, y, x);

	if (x == this->startX + static_cast<int32_t>(this->bufferSize))
		return;

	mvwdelch(this->main, y, x);
	::wmove(this->main, y, x);

	x -= this->startX;

	::memmove(this->commandBuffer + x, this->commandBuffer + x + 1, static_cast<int32_t>(this->bufferSize) - x);
	this->bufferSize--;

	if (this->bufferSize == 0UL)
		this->clearHints();
	else
		this->updateHints();

	this->refresh();
}

void InputTab::deleteCharBack(void) noexcept
{
	int32_t y, x;
	getyx(this->main, y, x);

	if (x == this->startX)
		return;

	mvwdelch(this->main, y, x - 1);
	::wmove(this->main, y, x - 1);

	x -= this->startX;

	::memmove(this->commandBuffer + x - 1, this->commandBuffer + x, static_cast<int32_t>(this->bufferSize) - x);
	this->bufferSize--;

	if (this->bufferSize == 0UL)
		this->clearHints();
	else
		this->updateHints();

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

	if (x < static_cast<int32_t>(this->startX + this->bufferSize))
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
	int32_t y, x;
	(void)y;
	getyx(this->main, y, x);

	x -= this->startX;

	if (x < static_cast<int32_t>(bufferSize))
		::memmove(this->commandBuffer + x + 1, this->commandBuffer + x, static_cast<int32_t>(this->bufferSize) - x);

	this->commandBuffer[x] = input;
	this->bufferSize++;

	if (bufferSize == Config::BUFF_SIZE)
		throw BufferOverflowException("Command buffer overflow");

	this->updateHints();		// got at least one input use hints now instead of history for suggestions
	this->showInput();
}

void InputTab::suggestNextCommand(void) noexcept
{
	if (this->autocompleteMode == false)
	{
		this->showPreviousCommand();
		return;
	}
	else if (this->hints.size() == 0UL)
		return;
	else if (this->currentSuggestedIndex == -1L)	//I'm typing a command, store it in tmpCommandBuffer and fetch the first hint
	{
		this->tmpBufferSize = this->bufferSize;
		::memcpy(this->tmpCommandBuffer, this->commandBuffer, this->tmpBufferSize);
	}
	if (this->currentSuggestedIndex < static_cast<ssize_t>(this->hints.size()) - 1L)
		this->currentSuggestedIndex++;

	const char* suggestedCommand = this->hints.at(this->currentSuggestedIndex);

	this->bufferSize = ::strlen(suggestedCommand); 
	::memcpy(this->commandBuffer, suggestedCommand, this->bufferSize);

	this->showInput();
}

void InputTab::showPreviousCommand(void) noexcept
{
	if ((this->history.size() == 0UL) or (this->currentCommandIndex == static_cast<ssize_t>(this->history.size()) - 1))
		return;
	else if (this->currentCommandIndex == -1L)	// I'm checking the last hint, now show the command I was typing stored in tmpCommandBuffer
	{
		this->tmpBufferSize = this->bufferSize;
		::memcpy(this->tmpCommandBuffer, this->commandBuffer, this->tmpBufferSize);
	}
	this->currentCommandIndex++;

	size_t& preCommandSize = this->history.at(this->currentCommandIndex).first;
	char* preCommand = this->history.at(this->currentCommandIndex).second.data();

	this->bufferSize = preCommandSize; 
	::memcpy(this->commandBuffer, preCommand, this->bufferSize);

	this->showInput();
}

void InputTab::suggestPastCommand(void) noexcept
{
	if (this->autocompleteMode == false)
	{
		this->showFollowingCommand();
		return;
	}
	else if ((this->hints.size() == 0UL) or (this->currentSuggestedIndex == -1L))
		return;
	
	if (this->currentSuggestedIndex > 0L)
	{
		this->currentSuggestedIndex--;
		
		const char* suggestedCommand = this->hints.at(this->currentSuggestedIndex);
		this->bufferSize = ::strlen(suggestedCommand); 
		::memcpy(this->commandBuffer, suggestedCommand, this->bufferSize);
	}
	else
	{
		this->currentSuggestedIndex = -1L;
		this->bufferSize = this->tmpBufferSize;
		::memcpy(this->commandBuffer, this->tmpCommandBuffer, this->bufferSize);
		this->tmpBufferSize = 0UL;
	}

	this->showInput();
}

void InputTab::showFollowingCommand(void) noexcept
{
	if (this->currentCommandIndex <= 0L)
	{
		this->currentCommandIndex = -1L;
		this->bufferSize = this->tmpBufferSize;
		::memcpy(this->commandBuffer, this->tmpCommandBuffer, this->bufferSize);
		this->tmpBufferSize = 0UL;
	}
	else
	{
		this->currentCommandIndex--;

		size_t& postCommandSize = this->history.at(this->currentCommandIndex).first;
		char* postCommand = this->history.at(this->currentCommandIndex).second.data();

		this->bufferSize = postCommandSize; 
		::memcpy(this->commandBuffer, postCommand, this->bufferSize);
	}

	this->showInput();
}

void InputTab::forwardCommand(void) noexcept
{
	if (this->bufferSize == 0UL)
		return;

	LOG_INFO(LogContext::INTERFACE, "Got new command: " + std::string(this->commandBuffer, this->bufferSize));

	if (this->sendCommandFd != -1)
		ioUtils::write(this->sendCommandFd, this->commandBuffer, this->bufferSize);

	this->history.emplace_front(this->bufferSize, std::array<char, Config::BUFF_SIZE>{});
	::memcpy(this->history.front().second.data(), this->commandBuffer, this->bufferSize);

	this->bufferSize = 0UL;
	// in case a command from history has been submitted reset the commandIndex to the last one inserted
	this->currentCommandIndex = -1L;
	this->clearHints();

	int32_t y, x;
	getyx(this->main, y, x);
	wmove(this->main, y + 1, 0);

	this->showInput();
}

void InputTab::appendContent(std::string const& newContent) noexcept
{
	int32_t y, x;
	getyx(this->main, y, x);

	if (x > this->startX)
		wmove(this->main, y + 1, 0);		// if there's some input go newline
	else
		wmove(this->main, y, 0);			// else override the prompt
	BasicTab::appendContent(newContent);
	waddstr(this->main, Config::PROMPT);
}

void InputTab::updateHints(void) noexcept
{
	this->hints.clear();
	for(const char* command : COMMANDS)
	{
		if (!::strncmp(command, this->commandBuffer, std::min(this->bufferSize, ::strlen(command))))
		{
			LOG_DEBUG(LogContext::INTERFACE, "found: " + std::string(command));
			this->hints.push_back(command);
		}
	}
	this->autocompleteMode = this->hints.empty() == false;
	this->currentSuggestedIndex = -1L;
}

void InputTab::clearHints(void) noexcept
{
	LOG_DEBUG(LogContext::INTERFACE, "ended");
	this->autocompleteMode = false;
	this->hints.clear();
}

void InputTab::showInput(void) const noexcept
{
	int32_t y, x;
	(void)x;
	getyx(this->main, y, x);

	::wmove(this->main, y, 0);
	::wclrtoeol(this->main);
	mvwaddstr(this->main, y, 0, Config::PROMPT);
	waddnstr(this->main, this->commandBuffer, this->bufferSize);
	this->refresh();
}

OutputTab::OutputTab(OutputTab&& other) noexcept :
	BasicTab(std::move(other)),
	content{std::move(other.content)},
	firstLineToPrintIndex{firstLineToPrintIndex}
{
}

OutputTab& OutputTab::operator=(OutputTab&& other) noexcept
{
	if (this != &other)
	{
		BasicTab::operator=(std::move(other));

		this->content = std::move(other.content);
		this->firstLineToPrintIndex = firstLineToPrintIndex;
	}
	return *this;
}

void OutputTab::appendContent(std::string const& newContent) noexcept
{
	this->content.push_back(newContent);

	int32_t h, w;
	(void)w;
	getmaxyx(this->main, h, w);

	if (static_cast<int32_t>(this->content.size()) > h)
	{
		this->firstLineToPrintIndex++;

		::wmove(this->main, 0, 0);
		for (int32_t i = 0; i < h; i++)
		{
			::wclrtoeol(this->main);
			BasicTab::appendContent(this->content.at(this->firstLineToPrintIndex + i));
		}
	}
	else
		BasicTab::appendContent(newContent);
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
	this->tabs[CommandLineUI::FRAME_TAB] = std::make_unique<BasicTab>(LINES, COLS, 0, 0, 0);
	this->tabs[CommandLineUI::FRAME_TAB]->appendContent("insert some shit, 'quit' to close");
	
	this->tabs[CommandLineUI::INPUT_TAB] = std::make_unique<InputTab>(LINES - 3, (COLS - 2) / 2, 2, 1, 0, this->commandPipe.in);
	this->tabs[CommandLineUI::INPUT_TAB]->appendContent("this is where the input is shown");

	this->tabs[CommandLineUI::OUTPUT_TAB] = std::make_unique<OutputTab>(LINES - 3, (COLS - 2) / 2, 2, (COLS - 2) / 2 + 1, 0);
	this->tabs[CommandLineUI::OUTPUT_TAB]->appendContent("this is where the output is shown");

	this->setCurrentTab(CommandLineUI::INPUT_TAB);
	this->getCurrentTab()->refresh();

	LOG_DEBUG(LogContext::INTERFACE, "Setup for CommandLine interface done");
}

void CommandLineUI::clear(void) noexcept
{
	this->tabs.clear();
	::endwin();

	LOG_INFO(LogContext::INTERFACE, "UI stopped");
}

void CommandLineUI::handleUserInput(void)
{
	InputTab* inputTab = dynamic_cast<InputTab*>(this->getCurrentTab());
	assert (inputTab != nullptr and "Using a tab which is not supposed to receive input");

	int32_t inputChar = inputTab->getCharInput();

	switch (inputChar)
	{
		case KEY_RESIZE:	// NB resize doesn't work
			LOG_DEBUG(LogContext::INTERFACE, "Resize window callback");
			break;

		case Config::COMMAND_TERM:
			inputTab->forwardCommand();
			break;

		case KEY_LEFT:
			inputTab->moveCursorLeft();
			break;
	
		case KEY_RIGHT:
			inputTab->moveCursorRight();
			break;

		case '\t':
			this->switchForwardTab();
			break;

		case KEY_BTAB:
			this->switchBackwardTab();
			break;
		
		case KEY_DC:
			inputTab->deleteCharForward();
			break;

		case 127:
		case KEY_BACKSPACE:
		{
			inputTab->deleteCharBack();
			break;
		}

		case KEY_UP:
			inputTab->suggestNextCommand();
			break;
			
		case KEY_DOWN:
			inputTab->suggestPastCommand();
			break;

		default:
			if (inputChar >= 32 && inputChar < 127)
				inputTab->storeCharInput(inputChar);
			break;
	}
}

void CommandLineUI::handleResponse(std::string const& response)
{
	this->tabs[OUTPUT_TAB]->appendContent(response.data());
}

void CommandLineUI::handleEvent(std::string const& event)
{
	this->tabs[OUTPUT_TAB]->appendContent(event.data());
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
