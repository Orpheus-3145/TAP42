#include "UI.hpp"
#include "Exceptions.hpp"
#include "ClientHTTP.hpp"

#include <string>
#include <cassert>


BasicTab::BasicTab(int32_t h, int32_t w, int32_t y, int32_t x, int32_t borderChar)
{
	if (borderChar > -1)
	{
		this->border = ::newwin(h, w, y, x);
		if (this->border == nullptr)
		{
			LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
			throw CliException("Failed to create window");
		}
		::box(this->border, borderChar, borderChar);
		::wnoutrefresh(this->border);

		h -= 2, w -= 2;
		y += 1, x += 1;
	}

	this->main = ::newwin(h, w, y, x);
	if (this->main == nullptr)
	{
		LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
		throw CliException("Failed to create window");
	}
	::keypad(this->main, true);

	::wnoutrefresh(this->main);
}

BasicTab::BasicTab(BasicTab&& other) noexcept :
	border{other.border},
	main{other.main},
	_state{std::move(other._state)}
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
		this->_state = std::move(other._state);

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
	this->_state.push_back(newContent);
	this->printLine(newContent);
}

void BasicTab::resize(int32_t newHeight, int32_t newWidth, int32_t newY, int32_t newX)
{
	int32_t y, x, h, w;
	if (this->border)
	{
		getmaxyx(this->border, h, w);
		if ((h == newHeight) and (w == newWidth))
			return;

		::werase(this->border);
		::wresize(this->border, newHeight, newWidth);

		getbegyx(this->border, y, x);
		if (((y != -1) or (x != -1)) and ((y != newY) or (x != newX)))
			mvwin(this->border, newY, newX);

		::box(this->border, 0, 0);		// NB store border char
		::wnoutrefresh(this->border);

		getmaxyx(this->main, h, w);
		// if ((h == newHeight) and (w == newWidth))
		// 	return;

		if (wresize(this->main, newHeight - 2, newWidth - 2) == ERR)
		::werase(this->main);
		getbegyx(this->main, y, x);

		if ((y != newY + 1) or (x != newX + 1))
			mvwin(this->main, newY + 1, newX + 1);
	}
	else
	{
		getmaxyx(this->main, h, w);
		if ((h == newHeight) and (w == newWidth))
			return;

		wresize(this->main, newHeight, newWidth);
		werase(this->main);
		getbegyx(this->main, y, x);

		if ((y != newY) or (x != newX))
			mvwin(this->main, newY, newX);
	}

	wmove(this->main, 0, 0);
	for (std::string const& line: this->_state)
		this->printLine(line);
}

void BasicTab::printLine(std::string const& newContent) const noexcept
{
	int32_t y, x;
	(void)x;
	getyx(this->main, y, x);

	waddstr(this->main, newContent.data());
	::wmove(this->main, y + 1, 0);

	this->refresh();
}


InputTab::InputTab(void) : BasicTab()
{
	::keypad(this->main, true);
	::waddstr(this->main, Config::PROMPT);

	::wnoutrefresh(this->main);
}

InputTab::InputTab(int32_t h, int32_t w, int32_t y, int32_t x, int32_t borderChar) :
	BasicTab(h, w, y, x, borderChar)
{
	::keypad(this->main, true);
	::waddstr(this->main, Config::PROMPT);

	::wnoutrefresh(this->main);
}

InputTab::InputTab(InputTab&& other) noexcept :
	BasicTab(std::move(other)),
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

int32_t InputTab::getChar(void) const noexcept
{
	return ::wgetch(this->main);
}

void InputTab::setChar(int32_t input)
{
	if (input != '\n')		//append normal char to buffer
	{
		int32_t y, x;
		(void)y;
		getyx(this->main, y, x);
	
		x -= this->startX;
	
		if (x < static_cast<int32_t>(this->bufferSize))			// in case the cursor is not at the end of the buffer
			::memmove(this->commandBuffer + x + 1, this->commandBuffer + x, static_cast<int32_t>(this->bufferSize) - x);
	
		this->commandBuffer[x] = static_cast<char>(input);		// could overflow if not ASCII value
		this->bufferSize++;
	
		if (this->bufferSize == Config::BUFF_SIZE)
			throw CliException("Command buffer overflow");
	
		this->updateHints();		// got at least one input use hints now instead of history for suggestions
	}
	else if (this->bufferSize > 0UL)		// if got end msg and buffer is not empty store current command
	{
		this->history.emplace_front(this->bufferSize, std::array<char, Config::BUFF_SIZE>{});
		::memcpy(this->history.front().second.data(), this->commandBuffer, this->bufferSize);

		this->bufferSize = 0UL;
		// in case a command from history has been submitted reset move commandIndex as the most recent command 
		this->currentCommandIndex = -1L;
		this->clearHints();

		int32_t y, x;
		getyx(this->main, y, x);
		wmove(this->main, y + 1, 0);
	}
	this->showInput();
}

void InputTab::suggestNextHint(void) noexcept
{
	if (this->autocompleteMode == false)
	{
		this->showPrevious();
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

void InputTab::showPrevious(void) noexcept
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

void InputTab::suggestPastHint(void) noexcept
{
	if (this->autocompleteMode == false)
	{
		this->showFollowing();
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

void InputTab::showFollowing(void) noexcept
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

std::string InputTab::getLastInput(void) const noexcept
{
	assert(this->history.empty() == false and "no input stored in history");
	return std::string(this->history.front().second.data(), this->history.back().first);
}

void InputTab::updateHints(void) noexcept
{
	this->hints.clear();
	for(const char* command : COMMANDS)
	{
		if (!::strncmp(command, this->commandBuffer, std::min(this->bufferSize, ::strlen(command))))
			this->hints.push_back(command);
	}
	this->autocompleteMode = this->hints.empty() == false;
	this->currentSuggestedIndex = -1L;
}

void InputTab::clearHints(void) noexcept
{
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


CommandLineUI::CommandLineUI(int32_t commandFd) :
	commandFd{commandFd}
{
	assert(this->commandFd != -1 and "Invalid fd from writing commands provided");

	// this->currentTabIndex = 0UL;

	// adjust window size
	// printf("\033[8;%d;%dt", height, width);
	// fflush(stdout);

	// (void) height;
	// (void) width;

	::initscr();
	::cbreak();
	::noecho();

	this->frame = std::make_unique<BasicTab>(LINES, COLS, 0, 0, 0);
	this->frame->appendContent("insert some shit, 'quit' to close");
	
	this->commandTab = std::make_unique<InputTab>(LINES - 3, (COLS - 2) / 2, 2, 1, 0);
	this->commandTab->appendContent("this is where the input is shown");

	this->responseTab = std::make_unique<OutputTab>((LINES - 3) / 2, (COLS - 2) / 2, 2, (COLS - 2) / 2 + 1, 0);
	this->responseTab->appendContent("this is where responses are shown");

	this->eventTab = std::make_unique<OutputTab>((LINES - 3) / 2, (COLS - 2) / 2, (LINES - 3) / 2 + 2, (COLS - 2) / 2 + 1, 0);
	this->eventTab->appendContent("this is where events are shown");

	this->commandTab->refresh();

	LOG_DEBUG(LogContext::INTERFACE, "Setup for CommandLine interface done");

	this->_dispatcher[KEY_LEFT]      = [this] { this->commandTab->moveCursorLeft(); };
	this->_dispatcher[KEY_RIGHT]     = [this] { this->commandTab->moveCursorRight(); };
	// this->_dispatcher['\t']          = [this] { this->switchForwardTab(); };
	// this->_dispatcher[KEY_BTAB]      = [this] { this->switchBackwardTab(); };
	// KEY_HOME / KEY_END: not mapped
	this->_dispatcher[KEY_DC]        = [this] { this->commandTab->deleteCharForward(); };
	this->_dispatcher[127]           = [this] { this->commandTab->deleteCharBack(); };
	this->_dispatcher[KEY_BACKSPACE] = [this] { this->commandTab->deleteCharBack(); };
	this->_dispatcher[KEY_UP]        = [this] { this->commandTab->suggestNextHint(); };
	this->_dispatcher[KEY_DOWN]      = [this] { this->commandTab->suggestPastHint(); };
}

CommandLineUI::~CommandLineUI(void) noexcept
{
	::endwin();

	LOG_INFO(LogContext::INTERFACE, "UI stopped");
}

	// ioUtils::Pipe resizePipe = ioUtils::createPipe();
	// signal_handler_fn = [resizePipe.in](int sig) {
	// 	LOG_DEBUG(LogContext::INTERFACE, "(output) got resize callback");
	// 	write(resizePipe.in, "x", 1);
    //     // qui puoi usare catture, perché è uno std::function
    // };
	//
    // std::signal(SIGWINCH, signal_handler_wrapper);
	//
    // signal(SIGWINCH, [](int fd) {
	// });
	//
	// if (fds[0].revents & POLLIN)
	// 	this->interface->handleUserInput();
	//
	// if (fds[1].revents & POLLIN)
	// {
	// 	char tmp;
	// 	write(resizePipe.out, &tmp, 1);
	//
	// 	struct winsize ws;
	// 	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	// 	resizeterm(ws.ws_row, ws.ws_col);
	//
	// 	LOG_DEBUG(LogContext::INTERFACE, "(input) got resize callback");
	// 	this->interface->resize();
	// }

void CommandLineUI::loop(void)
{
	this->refresh();
	while (this->KeepAlive == true)
	{
		int32_t input = this->commandTab->getChar();

		this->dispatch(input);
		this->refresh();
	}
}

void CommandLineUI::resize(void)
{
	// int32_t newHeight, newWidth;

	// getmaxyx(stdscr, newHeight, newWidth);

	// this->inputTabs[CommandLineUI::FRAME_TAB]->resize(newHeight, newWidth);

}

void CommandLineUI::dispatch(int32_t inputChar)
{
	auto it = this->_dispatcher.find(inputChar);
	if (it != this->_dispatcher.end()) {
		it->second();
		return;
	}

	// Default handling
	this->commandTab->setChar(inputChar);
	if (inputChar == COMMAND_TERM)
		this->forwardCommandToServer();
}

void CommandLineUI::forwardCommandToServer(void)
{
	std::string command = this->commandTab->getLastInput();
	LOG_INFO(LogContext::INTERFACE, "Got new command: " + command);

	ioUtils::write(this->commandFd, command.data(), command.size());

	if (command == QUIT)
		this->KeepAlive = false;

}

void CommandLineUI::handleResponse(std::string const& response)
{
	{
		std::lock_guard<std::mutex> lock(this->respMutex);
		this->responseTab->appendContent(response);
	}
	this->commandTab->refresh();
	this->refresh();		// manually refresh because main thread is polling
}

void CommandLineUI::handleEvent(std::string const& event)
{
	{
		std::lock_guard<std::mutex> lock(this->eventMutex);
		this->eventTab->appendContent(event);
	}
	this->commandTab->refresh();
	this->refresh();		// manually refresh because main thread is polling
}

// InputTab& CommandLineUI::getCurrentTab(void)
// {
// 	assert(this->currentTabIndex < this->inputTabs.size() and "index overflow whiele accessing input tabs");
// 	return this->inputTabs.at(this->currentTabIndex);
// }

// void CommandLineUI::switchForwardTab(void) noexcept
// {
// 	if (this->currentTabIndex < CommandLineUI::N_TABS - 1)
// 		this->currentTabIndex++;
// 	else
// 		this->currentTabIndex = 0UL;
// }

// void CommandLineUI::switchBackwardTab(void) noexcept
// {
// 	if (this->currentTabIndex > 0UL)
// 		this->currentTabIndex--;
// 	else
// 		this->currentTabIndex = CommandLineUI::N_TABS - 1;
// }

// void CommandLineUI::setCurrentTab(size_t newTabIndex) noexcept
// {
// 	assert(newTabIndex < CommandLineUI::N_TABS);
// 	this->currentTabIndex = newTabIndex;
// }
