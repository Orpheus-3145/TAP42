#include "CurseTab.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Exceptions.hpp"

#include <ncurses.h>
#include <cassert>


BasicTab::BasicTab(BasicTab&& other) noexcept :
	borderWin{other.borderWin},
	mainWin{other.mainWin},
	borderChar{other.borderChar},
	colorPair{other.colorPair},
	parent{other.parent},
	_state{std::move(other._state)}
{
	other.borderWin = nullptr;
	other.mainWin = nullptr;
}

BasicTab& BasicTab::operator=(BasicTab&& other) noexcept
{
	if (this != &other)
	{
		if (this->mainWin) ::delwin(this->mainWin);
		if (this->borderWin) ::delwin(this->borderWin);

		this->borderWin = other.borderWin;
		this->mainWin = other.mainWin;
		this->borderChar = other.borderChar;
		this->colorPair = other.colorPair;
		this->parent = other.parent;
		this->_state = std::move(other._state);

		other.borderWin = nullptr;
		other.mainWin = nullptr;
	}
	return *this;
}

void BasicTab::refresh(void) const noexcept
{
	::wnoutrefresh(this->borderWin);
	::wnoutrefresh(this->mainWin);
}

void BasicTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	assert((h > 0) and (w > 0) and "invalid size provided");
	assert((y > -1) and (x > -1) and "invalid position provided");

	if (this->borderChar != -1)
	{
		this->borderWin = ::newwin(h, w, y, x);
		if (this->borderWin == nullptr)
		{
			LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
			throw CliException("Failed to create window");
		}
		if (this->colorPair != -1)
			::wattron(this->borderWin, COLOR_PAIR(this->colorPair));
		::wborder(
			this->borderWin,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar
		);
		if (this->colorPair != -1)
			::wattroff(this->borderWin, COLOR_PAIR(this->colorPair));

		h -= 2, w -= 2;
		y += 1, x += 1;
	}

	this->mainWin = ::newwin(h, w, y, x);
	if (this->mainWin == nullptr)
	{
		LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
		throw CliException("Failed to create window");
	}

	::scrollok(this->mainWin, true);
	this->refresh();
}

void BasicTab::resize(int32_t h, int32_t w, int32_t y, int32_t x)
{
	this->clear();
	this->draw(h, w, y, x);

	for (std::string const& line: this->_state)
		this->printLine(line);
}

void BasicTab::appendContent(std::string const& newContent)
{
	this->_state.push_back(newContent);

	int32_t h, w;
	(void)w;
	getmaxyx(this->mainWin, h, w);

	if (static_cast<int32_t>(this->_state.size()) > h)
	{
		// reached the end of the tab, rotate le lines and drop the oldest one
		::wscrl(this->mainWin, 1);
		::wmove(this->mainWin, h - 1, 0);
		::wclrtoeol(this->mainWin);
	}
	this->printLine(newContent);
}

void BasicTab::printLine(std::string const& newContent) const noexcept
{
	int32_t y, x;
	(void)x;
	getyx(this->mainWin, y, x);

	waddstr(this->mainWin, newContent.data());
	::wmove(this->mainWin, y + 1, 0);

	this->refresh();
}

void BasicTab::clear(void) noexcept
{
	if (this->mainWin)
	{
		::wrefresh(this->mainWin); 
		::wclear(this->mainWin);
		::delwin(this->mainWin);
		this->mainWin = nullptr;
	}
	if (this->borderWin)
	{
		::wrefresh(this->borderWin);
		::wclear(this->borderWin);
		::delwin(this->borderWin);
		this->borderWin = nullptr;
	}
}


InputTab::InputTab(
	int32_t forwardInputFd,
	std::vector<std::string> const& hints,
	std::string const& prompt,
	int32_t borderChar,
	int32_t colorPair,
	CurseWindow* parent
) :
	BasicTab(borderChar, colorPair, parent),
	forwardInputFd{forwardInputFd},
	hints{hints},
	prompt{prompt}
{
	this->dispatcher[KEY_LEFT]      = [this] { this->moveCursorLeft(); };
	this->dispatcher[KEY_RIGHT]     = [this] { this->moveCursorRight(); };
	this->dispatcher[KEY_HOME]      = [this] { this->moveStartLine(); };
	this->dispatcher[KEY_END]       = [this] { this->moveEndLine(); };
	this->dispatcher[KEY_DC]        = [this] { this->deleteCharForward(); };
	this->dispatcher[127]           = [this] { this->deleteCharBack(); };
	this->dispatcher[KEY_BACKSPACE] = [this] { this->deleteCharBack(); };
	this->dispatcher[KEY_UP]        = [this] { this->showPrevious(); };
	this->dispatcher[KEY_DOWN]      = [this] { this->showNext(); };
	this->dispatcher['\t']          = [this] { this->suggestHint(); };
	this->dispatcher[KEY_BTAB]      = [this] { if (this->parent) this->parent->switchInputTab(); };
}

InputTab::InputTab(InputTab&& other) noexcept :
	BasicTab(std::move(other)),
	forwardInputFd{other.forwardInputFd},
	hints{std::move(other.hints)},
	prompt{std::move(other.prompt)},
	dispatcher{std::move(other.dispatcher)},
	history{std::move(other.history)},
	suggestedHintIndexes{std::move(other.suggestedHintIndexes)},
	currentCommandIndex{other.currentCommandIndex},
	currentSuggestedIndex{other.currentSuggestedIndex},
	startX{other.startX},
	bufferSize{other.bufferSize},
	tmpBufferSize{other.tmpBufferSize},
	autocompleteMode{other.autocompleteMode}
{
	::memmove(this->commandBuffer, other.commandBuffer, this->bufferSize);
	::memmove(this->tmpCommandBuffer, other.tmpCommandBuffer, this->tmpBufferSize);
}

void InputTab::handleUserInput(void)
{
	int32_t inputChar = ::wgetch(this->mainWin);	// this is blocking

	// special characters handling
	auto it = this->dispatcher.find(inputChar);
	if (it != this->dispatcher.end())
	{
		it->second();
		return;
	}

	// Default handling
	this->setChar(inputChar);
}

void InputTab::deleteCharForward(void) noexcept
{
	int32_t y, x;
	(void)y;
	getyx(this->mainWin, y, x);

	if (x == this->startX + static_cast<int32_t>(this->bufferSize))		// end of the line can't do delete
		return;

	mvwdelch(this->mainWin, y, x);

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
	getyx(this->mainWin, y, x);

	if (x == this->startX)			// start of the line cant't do backspace
		return;

	mvwdelch(this->mainWin, y, x - 1);

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

	getyx(this->mainWin, y, x);

	if (x > this->startX)
	{
		::wmove(this->mainWin, y, x - 1);
		this->refresh();
	}
}

void InputTab::moveCursorRight(void) const noexcept
{
	int32_t y, x;
	(void)y;
	
	getyx(this->mainWin, y, x);

	if (x < static_cast<int32_t>(this->startX + this->bufferSize))
	{
		::wmove(this->mainWin, y, x + 1);
		this->refresh();
	}
}

void InputTab::moveStartLine(void) const noexcept
{
	int32_t y, x;
	(void)x;

	getyx(this->mainWin, y, x);
	::wmove(this->mainWin, y, this->startX);
	this->refresh();
}

void InputTab::moveEndLine(void) const noexcept
{
	int32_t y, x;
	(void)x;

	getyx(this->mainWin, y, x);
	::wmove(this->mainWin, y, this->startX + this->bufferSize);
	this->refresh();
}

void InputTab::setChar(int32_t input)
{
	if (input != Config::COMMAND_TERM)		// append normal char to buffer
		this->appendCharToInput(input);
	else							// if got end msg and buffer is not empty store current command
		this->terminateInput();
}

void InputTab::suggestHint(void) noexcept
{
	if (this->suggestedHintIndexes.size() == 0UL)
		return;
	else if (this->currentSuggestedIndex == -1L)	// store the current command it in tmpCommandBuffer and fetch the first hint
	{
		this->tmpBufferSize = this->bufferSize;
		::memcpy(this->tmpCommandBuffer, this->commandBuffer, this->tmpBufferSize);
		this->currentSuggestedIndex++;
	}

	if (this->currentSuggestedIndex < static_cast<ssize_t>(this->suggestedHintIndexes.size()))
	{
		uint32_t suggestedHintIndex = this->suggestedHintIndexes.at(this->currentSuggestedIndex);
		std::string const& suggestedHint = this->hints.at(suggestedHintIndex);
		
		this->bufferSize = suggestedHint.size(); 
		::memcpy(this->commandBuffer, suggestedHint.data(), this->bufferSize);
		this->currentSuggestedIndex++;
	}
	else			// show command previously typed
	{
		this->currentSuggestedIndex = -1L;
		this->bufferSize = this->tmpBufferSize;
		::memcpy(this->commandBuffer, this->tmpCommandBuffer, this->bufferSize);
		this->tmpBufferSize = 0UL;
	}

	this->writePromptLine();
}

void InputTab::suggestPrevious(void) noexcept
{
	if (this->autocompleteMode == false)
	{
		this->showPrevious();
		return;
	}
	else if (this->suggestedHintIndexes.size() == 0UL)
		return;
	else if (this->currentSuggestedIndex == -1L)	// store the current command it in tmpCommandBuffer and fetch the first hint
	{
		this->tmpBufferSize = this->bufferSize;
		::memcpy(this->tmpCommandBuffer, this->commandBuffer, this->tmpBufferSize);
	}
	if (this->currentSuggestedIndex < static_cast<ssize_t>(this->suggestedHintIndexes.size()) - 1L)
		this->currentSuggestedIndex++;

	uint32_t suggestedHintIndex = this->suggestedHintIndexes.at(this->currentSuggestedIndex);
	std::string const& suggestedHint = this->hints.at(suggestedHintIndex);

	this->bufferSize = suggestedHint.size(); 
	::memcpy(this->commandBuffer, suggestedHint.data(), this->bufferSize);

	this->writePromptLine();
}

void InputTab::showPrevious(void) noexcept
{
	ssize_t nHistoryItems = this->history.size();
	if ((nHistoryItems == 0L) or (this->currentCommandIndex == nHistoryItems - 1))
		return;		// skip if history empty or current shown element it the oldest in the history
	else if (this->currentCommandIndex == -1L)
	{				// currently showing the most recent command, the next was the one being typed before switching
		this->tmpBufferSize = this->bufferSize;
		::memcpy(this->tmpCommandBuffer, this->commandBuffer, this->tmpBufferSize);
	}
	this->currentCommandIndex++;

	std::string const& previousCommand = this->history.at(this->currentCommandIndex);
	this->bufferSize = previousCommand.size(); 
	::memcpy(this->commandBuffer, previousCommand.data(), this->bufferSize);

	this->writePromptLine();
}

void InputTab::suggestNext(void) noexcept
{
	if (this->autocompleteMode == false)
	{
		this->showNext();
		return;
	}
	else if ((this->suggestedHintIndexes.size() == 0UL) or (this->currentSuggestedIndex == -1L))
		return;
	
	if (this->currentSuggestedIndex > 0L)
	{
		this->currentSuggestedIndex--;

		uint32_t suggestedHintIndex = this->suggestedHintIndexes.at(this->currentSuggestedIndex);
		std::string const& suggestedHint = this->hints.at(suggestedHintIndex);

		this->bufferSize = suggestedHint.size(); 
		::memcpy(this->commandBuffer, suggestedHint.data(), this->bufferSize);
	}
	else
	{
		this->currentSuggestedIndex = -1L;
		this->bufferSize = this->tmpBufferSize;
		::memcpy(this->commandBuffer, this->tmpCommandBuffer, this->bufferSize);
		this->tmpBufferSize = 0UL;
	}

	this->writePromptLine();
}

void InputTab::showNext(void) noexcept
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

		std::string const& previousCommand = this->history.at(this->currentCommandIndex);
		this->bufferSize = previousCommand.size(); 
		::memcpy(this->commandBuffer, previousCommand.data(), this->bufferSize);
	}

	this->writePromptLine();
}

void InputTab::appendContent(std::string const& newContent)
{
	int32_t y, x;
	getyx(this->mainWin, y, x);

	if (x > this->startX)					// if there's some input go to newline
		wmove(this->mainWin, y + 1, 0);
	else									// else override the prompt
	{
		wmove(this->mainWin, y, 0);
		::wclrtoeol(this->mainWin);
	}

	BasicTab::appendContent(newContent);
	this->writePromptLine();
}

void InputTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	BasicTab::draw(h, w, y, x);
	::keypad(this->mainWin, true);

	this->writePromptLine();
	this->refresh();
}

void InputTab::updateHints(void) noexcept
{
	this->suggestedHintIndexes.clear();
	for(uint32_t i = 0U; i < this->hints.size(); i++)
	{
		std::string const& currentHint = this->hints.at(i);
		if (::strncmp(currentHint.data(), this->commandBuffer, std::min(this->bufferSize, currentHint.size())))
			continue;

		this->suggestedHintIndexes.push_back(i);
	}
	this->autocompleteMode = this->suggestedHintIndexes.empty() == false;
	this->currentSuggestedIndex = -1L;
}

void InputTab::clearHints(void) noexcept
{
	this->autocompleteMode = false;
	this->suggestedHintIndexes.clear();
}

void InputTab::writePromptLine(void) const noexcept
{
	int32_t y, x;
	(void)x;
	getyx(this->mainWin, y, x);

	::wmove(this->mainWin, y, 0);
	::wclrtoeol(this->mainWin);

	if (this->colorPair != -1)
		::wattron(this->mainWin, COLOR_PAIR(this->colorPair) | A_BLINK);
	else
		::wattron(this->mainWin, A_BLINK);

	waddstr(this->mainWin, this->prompt.data());

	if (this->colorPair != -1)
		::wattroff(this->mainWin, COLOR_PAIR(this->colorPair) | A_BLINK);
	else
		::wattroff(this->mainWin, A_BLINK);

	if (this->bufferSize > 0UL)
		waddnstr(this->mainWin, this->commandBuffer, this->bufferSize);
	this->refresh();
}

void InputTab::appendCharToInput(int32_t input)
{
	if (this->bufferSize == Config::CMD_BUFFER_SIZE)
		throw CliException("Command buffer overflow");

	bool resetCursorPos = false;
	int32_t y, x;
	(void) y;
	getyx(this->mainWin, y, x);
	x -= this->startX;

	if (x < static_cast<int32_t>(this->bufferSize))			// in case the cursor is not at the end of the buffer
	{
		resetCursorPos = true;
		::memmove(this->commandBuffer + x + 1, this->commandBuffer + x, static_cast<int32_t>(this->bufferSize) - x);
	}

	this->commandBuffer[x] = static_cast<char>(input);		// could overflow if not ASCII value
	this->bufferSize++;

	this->updateHints();		// got at least one input use hints now instead of history for suggestions
	this->writePromptLine();
	
	// move cursor back where it was originally
	if (resetCursorPos)
	{
		::wmove(this->mainWin, y, x + this->startX + 1);
		this->refresh();
	}
}

void InputTab::terminateInput(void)
{
	if (this->bufferSize == 0UL)
		return;

	std::string command = std::string(this->commandBuffer, this->bufferSize);
	ioUtils::write(this->forwardInputFd, command.data(), command.size());

	this->_state.push_back(this->prompt + command);
	this->history.emplace_front(std::move(command));

	this->bufferSize = 0UL;
	// in case a command from history has been submitted reset move commandIndex as the most recent command 
	this->currentCommandIndex = -1L;
	this->clearHints();

	int32_t y, x;
	getyx(this->mainWin, y, x);
	wmove(this->mainWin, y + 1, 0);
	this->writePromptLine();
}


void SingleInputTab::terminateInput(void)
{
	if (this->bufferSize == 0UL)
		return;

	std::string command = std::string(this->commandBuffer, this->bufferSize);
	ioUtils::write(this->forwardInputFd, command.data(), command.size());

	this->history.emplace_front(std::move(command));

	this->bufferSize = 0UL;
	// in case a command from history has been submitted reset move commandIndex as the most recent command 
	this->currentCommandIndex = -1L;

	int32_t y, x;
	getyx(this->mainWin, y, x);
	wmove(this->mainWin, y, 0);

	this->clearHints();
	this->writePromptLine();
}


OutputTab::OutputTab(OutputTab&& other) noexcept :
	BasicTab(std::move(other)),
	titleWin{other.titleWin},
	title{std::move(other.title)}
{
	other.titleWin = nullptr;
}

OutputTab& OutputTab::operator=(OutputTab&& other) noexcept
{
	if (this != &other)
	{
		BasicTab::operator=(std::move(other));

		this->titleWin = other.titleWin;
		this->title = std::move(other.title);

		other.titleWin = nullptr;
	}
	return *this;
}

void OutputTab::refresh(void) const noexcept
{
	::wnoutrefresh(this->borderWin);
	::wnoutrefresh(this->titleWin);
	::wnoutrefresh(this->mainWin);
}

void OutputTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	BasicTab::draw(h, w, y, x);

	if (this->title.empty() == false)
	{
		if (this->borderWin != nullptr)		// if there's a border adjust position and size of the title
		{
			h -= 2;
			w -= 2;
			y += 1;
			x += 1;
		}
		assert((w + 1) > static_cast<int32_t>(this->title.size()) and "Tab title longer than tab itself");
		// adding a title
		this->titleWin = ::newwin(3, this->title.size() + 2, y, x + 1);
		if (this->borderWin == nullptr)
		{
			LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
			throw CliException("Failed to create window");
		}

		if (this->colorPair != -1)
			::wattron(this->titleWin, COLOR_PAIR(this->colorPair) | A_BOLD);
		else
			::wattron(this->titleWin, A_BOLD);

		::wborder(this->titleWin, 0, 0, 0, 0, 0, 0, 0, 0);
		mvwaddstr(this->titleWin, 1, 1, this->title.data());

		if (this->colorPair != -1)
			::wattroff(this->titleWin, COLOR_PAIR(this->colorPair) | A_BOLD);
		else
			::wattroff(this->titleWin, A_BOLD);

		// readjust position and size of mainWin
		::mvwin(this->mainWin, y + 3, x + 1);
		::wresize(this->mainWin, h - 3, w - 2);

		// add a div line between title and ouput
		if (this->colorPair != -1)
			::wattron(this->mainWin, COLOR_PAIR(this->colorPair));
		::wborder(this->mainWin, ' ', ' ', 0, ' ', ACS_HLINE, ACS_HLINE, ' ', ' ');
		if (this->colorPair != -1)
			::wattroff(this->mainWin, COLOR_PAIR(this->colorPair));

		int32_t y, x;
		getyx(this->mainWin, y, x);
		::wmove(this->mainWin, y + 1, 0);
	}
	this->refresh();
}

void OutputTab::clear(void) noexcept
{
	BasicTab::clear();

	if (this->titleWin)
	{
		::wrefresh(this->titleWin);
		::wclear(this->titleWin);
		::delwin(this->titleWin);
		this->titleWin = nullptr;
	}
}
