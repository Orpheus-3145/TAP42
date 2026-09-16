#include "CurseTab.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Exceptions.hpp"

#include <ncurses.h>
#include <cassert>


BasicTab::BasicTab(BasicTab&& other) noexcept :
	border{other.border},
	main{other.main},
	borderChar{other.borderChar},
	parent{other.parent},
	_state{std::move(other._state)}
{
	other.border = nullptr;
	other.main = nullptr;
}

BasicTab& BasicTab::operator=(BasicTab&& other) noexcept
{
	if (this != &other)
	{
		if (this->main) ::delwin(this->main);
		if (this->border) ::delwin(this->border);

		this->border = other.border;
		this->main = other.main;
		this->borderChar = other.borderChar;
		this->parent = other.parent;
		this->_state = std::move(other._state);

		other.border = nullptr;
		other.main = nullptr;
	}
	return *this;
}

void BasicTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	if (this->borderChar != -1)
	{
		this->border = ::newwin(h, w, y, x);
		if (this->border == nullptr)
		{
			LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
			throw CliException("Failed to create window");
		}
		::wborder(
			this->border,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar
		);
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

	for (std::string const& line: this->_state)
		this->printLine(line);

	::wnoutrefresh(this->main);
}

void BasicTab::appendContent(std::string const& newContent) noexcept
{
	this->_state.push_back(newContent);
	this->printLine(newContent);
}

void BasicTab::resize(int32_t h, int32_t w, int32_t y, int32_t x)
{
	assert((h > 0) and (w > 0) and "Invalid resizing size provided");
	assert((y > -1) and (x > -1) and "Invalid resizing position provided");

	this->clear();
	this->draw(h, w, y, x);
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

void BasicTab::clear(void) noexcept
{
	if (this->main)
	{
		::wrefresh(this->main); 
		::wclear(this->main);
		::delwin(this->main);
		this->main = nullptr;
	}
	if (this->border)
	{
		::wrefresh(this->border);
		::wclear(this->border);
		::delwin(this->border);
		this->border = nullptr;
	}
}


InputTab::InputTab(int32_t forwardInputFd, int32_t borderChar, CurseWindow* parent) :
	BasicTab(borderChar, parent),
	forwardInputFd{forwardInputFd}
{
	this->dispatcher[KEY_LEFT]      = [this] { this->moveCursorLeft(); };
	this->dispatcher[KEY_RIGHT]     = [this] { this->moveCursorRight(); };
	this->dispatcher[KEY_HOME]      = [this] { this->moveStartLine(); };
	this->dispatcher[KEY_END]       = [this] { this->moveEndLine(); };
	this->dispatcher[KEY_DC]        = [this] { this->deleteCharForward(); };
	this->dispatcher[127]           = [this] { this->deleteCharBack(); };
	this->dispatcher[KEY_BACKSPACE] = [this] { this->deleteCharBack(); };
	this->dispatcher[KEY_UP]        = [this] { this->suggestPrevious(); };
	this->dispatcher[KEY_DOWN]      = [this] { this->suggestNext(); };
	this->dispatcher['\t']          = [this] { if (this->parent) this->parent->switchNextTab(); };
	this->dispatcher[KEY_BTAB]      = [this] { if (this->parent) this->parent->switchPreviousTab(); };
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

void InputTab::handleUserInput(void)
{
	int32_t inputChar = ::wgetch(this->main);	// this is blocking

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
	getyx(this->main, y, x);

	if (x == this->startX + static_cast<int32_t>(this->bufferSize))		// end of the line can't do delete
		return;

	mvwdelch(this->main, y, x);

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

	if (x == this->startX)			// start of the line cant't do backspace
		return;

	mvwdelch(this->main, y, x - 1);

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
	{
		::wmove(this->main, y, x - 1);
		this->refresh();
	}
}

void InputTab::moveCursorRight(void) const noexcept
{
	int32_t y, x;
	(void)y;
	
	getyx(this->main, y, x);

	if (x < static_cast<int32_t>(this->startX + this->bufferSize))
	{
		::wmove(this->main, y, x + 1);
		this->refresh();
	}
}

void InputTab::moveStartLine(void) const noexcept
{
	int32_t y, x;
	(void)x;

	getyx(this->main, y, x);
	::wmove(this->main, y, this->startX);
	this->refresh();
}

void InputTab::moveEndLine(void) const noexcept
{
	int32_t y, x;
	(void)x;

	getyx(this->main, y, x);
	::wmove(this->main, y, this->startX + this->bufferSize);
	this->refresh();
}

void InputTab::setChar(int32_t input)
{
	if (input != COMMAND_TERM)		// append normal char to buffer
		this->appendCharToInput(input);
	else							// if got end msg and buffer is not empty store current command
		this->terminateInput();
}

void InputTab::suggestPrevious(void) noexcept
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

	this->overwriteLine();
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

	std::string const& previousCommand = this->history.at(this->currentCommandIndex);
	this->bufferSize = previousCommand.size(); 
	::memcpy(this->commandBuffer, previousCommand.data(), this->bufferSize);

	this->overwriteLine();
}

void InputTab::suggestNext(void) noexcept
{
	if (this->autocompleteMode == false)
	{
		this->showNext();
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

	this->overwriteLine();
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

	this->overwriteLine();
}

void InputTab::appendContent(std::string const& newContent) noexcept
{
	int32_t y, x;
	getyx(this->main, y, x);

	if (x > this->startX)
		wmove(this->main, y + 1, 0);		// if there's some input go to newline
	else
		wmove(this->main, y, 0);			// else override the prompt

	BasicTab::appendContent(newContent);
	waddstr(this->main, PROMPT);
}

void InputTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	BasicTab::draw(h, w, y, x);
	::keypad(this->main, true);

	::waddstr(this->main, PROMPT);
	if (this->bufferSize > 0UL)
		::waddnstr(this->main, this->commandBuffer, this->bufferSize);

	::wnoutrefresh(this->main);
}

void InputTab::updateHints(void) noexcept
{
	this->hints.clear();
	for(const char* hintCommand : HINTS)
	{
		if (!::strncmp(hintCommand, this->commandBuffer, std::min(this->bufferSize, ::strlen(hintCommand))))
			this->hints.push_back(hintCommand);
	}
	this->autocompleteMode = this->hints.empty() == false;
	this->currentSuggestedIndex = -1L;
}

void InputTab::clearHints(void) noexcept
{
	this->autocompleteMode = false;
	this->hints.clear();
}

void InputTab::overwriteLine(void) const noexcept
{
	int32_t y, x;
	(void)x;
	getyx(this->main, y, x);

	::wmove(this->main, y, 0);
	::wclrtoeol(this->main);
	mvwaddstr(this->main, y, 0, PROMPT);
	waddnstr(this->main, this->commandBuffer, this->bufferSize);
	this->refresh();
}

void InputTab::appendCharToInput(int32_t input)
{
	if (this->bufferSize == Config::CMD_BUFFER_SIZE)
		throw CliException("Command buffer overflow");

	bool resetCursorPos = false;
	int32_t y, x;
	(void) y;
	getyx(this->main, y, x);
	x -= this->startX;

	if (x < static_cast<int32_t>(this->bufferSize))			// in case the cursor is not at the end of the buffer
	{
		resetCursorPos = true;
		::memmove(this->commandBuffer + x + 1, this->commandBuffer + x, static_cast<int32_t>(this->bufferSize) - x);
	}

	this->commandBuffer[x] = static_cast<char>(input);		// could overflow if not ASCII value
	this->bufferSize++;

	this->updateHints();		// got at least one input use hints now instead of history for suggestions
	this->overwriteLine();
	
	// move cursor back where it was originally
	if (resetCursorPos)
	{
		::wmove(this->main, y, x + this->startX + 1);
		this->refresh();
	}
}

void InputTab::terminateInput(void)
{
	if (this->bufferSize == 0UL)
		return;

	std::string command = std::string(this->commandBuffer, this->bufferSize);
	ioUtils::write(this->forwardInputFd, command.data(), command.size());

	this->_state.push_back(PROMPT + command);
	this->history.emplace_front(std::move(command));

	this->bufferSize = 0UL;
	// in case a command from history has been submitted reset move commandIndex as the most recent command 
	this->currentCommandIndex = -1L;
	this->clearHints();

	int32_t y, x;
	getyx(this->main, y, x);
	wmove(this->main, y + 1, 0);
	this->overwriteLine();
}


void SingleInputTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	if (this->borderChar != -1)
	{
		this->border = ::newwin(h, w, y, x);
		if (this->border == nullptr)
		{
			LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
			throw CliException("Failed to create window");
		}
		::wborder(
			this->border,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar,
			this->borderChar
		);
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

	::waddstr(this->main, PROMPT);
	if (this->bufferSize > 0UL)
		::waddnstr(this->main, this->commandBuffer, this->bufferSize);

	::wnoutrefresh(this->main);
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
	getyx(this->main, y, x);
	wmove(this->main, y, 0);

	this->clearHints();
	this->overwriteLine();
}


OutputTab::OutputTab(OutputTab&& other) noexcept :
	BasicTab(std::move(other)),
	content{std::move(other.content)},
	firstLineToPrintIndex{other.firstLineToPrintIndex}
{
}

OutputTab& OutputTab::operator=(OutputTab&& other) noexcept
{
	if (this != &other)
	{
		BasicTab::operator=(std::move(other));

		this->content = std::move(other.content);
		this->firstLineToPrintIndex = other.firstLineToPrintIndex;
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
		// reached the end of the tab, remove the latest input and print the newer ones
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
