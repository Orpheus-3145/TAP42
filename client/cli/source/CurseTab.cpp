#include "CurseTab.hpp"
#include "UI.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


BasicTab::BasicTab(int32_t borderChar, int32_t colorPair, CurseWindow* parent) : 
	borderChar{borderChar},
	colorPair{colorPair},
	parent{parent}
{
	if (::has_colors() == false)
		this->colorPair = -1;

	this->dispatcher[KEY_BTAB] = [this] { if (this->parent) this->parent->switchActiveTab(); };
}

BasicTab::BasicTab(BasicTab&& other) noexcept :
	borderWin{other.borderWin},
	borderChar{other.borderChar},
	colorPair{other.colorPair},
	parent{other.parent},
	dispatcher{std::move(other.dispatcher)},
	isActive{other.isActive}
{
	other.borderWin = nullptr;
}

BasicTab& BasicTab::operator=(BasicTab&& other) noexcept
{
	if (this != &other)
	{
		if (this->borderWin) ::delwin(this->borderWin);

		this->borderWin = other.borderWin;
		this->borderChar = other.borderChar;
		this->colorPair = other.colorPair;
		this->parent = other.parent;
		this->dispatcher = std::move(other.dispatcher);
		this->isActive = other.isActive;

		other.borderWin = nullptr;
	}
	return *this;
}

void BasicTab::refresh(void) const noexcept
{
	::wnoutrefresh(this->borderWin);
}

void BasicTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	assert((h > 0) and (w > 0) and "invalid size provided");
	assert((y > -1) and (x > -1) and "invalid position provided");

	this->borderWin = ::newwin(h, w, y, x);
	if (this->borderWin == nullptr)
	{
		LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
		throw CliException("Failed to create window");
	}
	if (this->borderChar != -1)
	{
		if (this->colorPair != -1)
			::wattron(this->borderWin, COLOR_PAIR(this->colorPair));
		::box(this->borderWin, this->borderChar, this->borderChar);
		if (this->colorPair != -1)
			::wattroff(this->borderWin, COLOR_PAIR(this->colorPair));
	}
	::keypad(this->borderWin, true);
	this->refresh();
}

void BasicTab::resize(int32_t h, int32_t w, int32_t y, int32_t x)
{
	this->clear();
	this->draw(h, w, y, x);
}

void BasicTab::clear(void) noexcept
{
	::wrefresh(this->borderWin);
	::wclear(this->borderWin);
	::delwin(this->borderWin);
	this->borderWin = nullptr;
}

void BasicTab::handleUserInput(void)
{
	int32_t inputChar = ::wgetch(this->borderWin);	// this is blocking

	// ncurses throws many KEY_RESIZE when term is resized, ignore them
	// since the resizing is handled by catching SIGWINCH
	if (inputChar == KEY_RESIZE)
		return;

	// special characters handling
	auto it = this->dispatcher.find(inputChar);
	if (it != this->dispatcher.end())
		it->second();
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
}

InputTab::InputTab(InputTab&& other) noexcept :
	BasicTab(std::move(other)),
	forwardInputFd{other.forwardInputFd},
	hints{std::move(other.hints)},
	prompt{std::move(other.prompt)},
	inputWin{other.inputWin},
	inputHistory{std::move(other.inputHistory)},
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

	other.inputWin = nullptr;
}

void InputTab::handleUserInput(void)
{
	int32_t inputChar = ::wgetch(this->inputWin);	// this is blocking

	// ncurses throws many KEY_RESIZE when term is resized, ignore them
	// since the resizing is handled by catching SIGWINCH
	if (inputChar == KEY_RESIZE)
		return;

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
	getyx(this->inputWin, y, x);

	if (x == this->startX + static_cast<int32_t>(this->bufferSize))		// end of the line can't do delete
		return;

	mvwdelch(this->inputWin, y, x);

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
	getyx(this->inputWin, y, x);

	if (x == this->startX)			// start of the line cant't do backspace
		return;

	mvwdelch(this->inputWin, y, x - 1);

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

	getyx(this->inputWin, y, x);

	if (x > this->startX)
	{
		::wmove(this->inputWin, y, x - 1);
		this->refresh();
	}
}

void InputTab::moveCursorRight(void) const noexcept
{
	int32_t y, x;
	(void)y;
	
	getyx(this->inputWin, y, x);

	if (x < static_cast<int32_t>(this->startX + this->bufferSize))
	{
		::wmove(this->inputWin, y, x + 1);
		this->refresh();
	}
}

void InputTab::moveStartLine(void) const noexcept
{
	int32_t y, x;
	(void)x;

	getyx(this->inputWin, y, x);
	::wmove(this->inputWin, y, this->startX);
	this->refresh();
}

void InputTab::moveEndLine(void) const noexcept
{
	int32_t y, x;
	(void)x;

	getyx(this->inputWin, y, x);
	::wmove(this->inputWin, y, this->startX + this->bufferSize);
	this->refresh();
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
	ssize_t nHistoryItems = this->inputHistory.size();
	if ((nHistoryItems == 0L) or (this->currentCommandIndex == nHistoryItems - 1))
		return;		// skip if history empty or current shown element it the oldest in the history
	else if (this->currentCommandIndex == -1L)
	{				// currently showing the most recent command, the next was the one being typed before switching
		this->tmpBufferSize = this->bufferSize;
		::memcpy(this->tmpCommandBuffer, this->commandBuffer, this->tmpBufferSize);
	}
	this->currentCommandIndex++;

	std::string const& previousCommand = this->inputHistory.at(this->currentCommandIndex);
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

		std::string const& previousCommand = this->inputHistory.at(this->currentCommandIndex);
		this->bufferSize = previousCommand.size(); 
		::memcpy(this->commandBuffer, previousCommand.data(), this->bufferSize);
	}

	this->writePromptLine();
}

void InputTab::refresh(void) const noexcept
{
	BasicTab::refresh();
	::wnoutrefresh(this->inputWin);
}

void InputTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	BasicTab::draw(h, w, y, x);

	if (this->borderWin != nullptr)		// if there's a border adjust position and size of the title
	{
		h -= 2;
		w -= 2;
		y += 1;
		x += 1;
	}
	this->inputWin = ::newwin(h, w, y, x);
	if (this->inputWin == nullptr)
	{
		LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
		throw CliException("Failed to create window");
	}

	::keypad(this->inputWin, true);
	this->writePromptLine();
}

void InputTab::clear(void) noexcept
{
	BasicTab::clear();

	::wrefresh(this->inputWin);
	::wclear(this->inputWin);
	::delwin(this->inputWin);
	this->inputWin = nullptr;
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
	getyx(this->inputWin, y, x);

	::wmove(this->inputWin, y, 0);
	::wclrtoeol(this->inputWin);

	if (this->colorPair != -1)
		::wattron(this->inputWin, COLOR_PAIR(this->colorPair));
	if (this->isActive == true)
		::wattron(this->inputWin, A_BLINK);
	::wattron(this->inputWin, A_BOLD);

	waddstr(this->inputWin, this->prompt.data());

	if (this->colorPair != -1)
		::wattroff(this->inputWin, COLOR_PAIR(this->colorPair));
	if (this->isActive == true)
		::wattroff(this->inputWin, A_BLINK);
	::wattroff(this->inputWin, A_BOLD);

	if (this->bufferSize > 0UL)
		waddnstr(this->inputWin, this->commandBuffer, this->bufferSize);

	this->refresh();
}

void InputTab::activate(void)
{
	BasicTab::activate();

	if (this->colorPair != -1)
		::wattron(this->inputWin, COLOR_PAIR(this->colorPair));
	::wattron(this->inputWin, A_BOLD | A_BLINK);

	::box(this->inputWin, 0, 0);

	if (this->colorPair != -1)
		::wattroff(this->inputWin, COLOR_PAIR(this->colorPair));
	::wattroff(this->inputWin, A_BOLD | A_BLINK);

	this->writePromptLine();
}

void InputTab::deactivate(void)
{
	BasicTab::deactivate();

	if (this->colorPair != -1)
		::wattron(this->inputWin, COLOR_PAIR(this->colorPair));
	::wattron(this->inputWin, A_BOLD);

	::box(this->inputWin, 0, 0);

	if (this->colorPair != -1)
		::wattroff(this->inputWin, COLOR_PAIR(this->colorPair));
	::wattroff(this->inputWin, A_BOLD);

	this->writePromptLine();
}

void InputTab::setChar(int32_t input)
{
	if (input != COMMAND_TERM)		// append normal char to buffer
		this->appendInputChar(input);
	else							// if got end msg and buffer is not empty store current command
		this->terminateInput();
}

void InputTab::appendInputChar(int32_t input)
{
	int32_t y, x;
	bool resetCursorPos = false;

	if (this->bufferSize == Config::CMD_BUFFER_SIZE)
		throw CliException("Command buffer overflow");

	getyx(this->inputWin, y, x);
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
		::wmove(this->inputWin, y, x + this->startX + 1);
		this->refresh();
	}
}

void InputTab::terminateInput(void)
{
	if (this->bufferSize == 0UL)
		return;

	int32_t y, x;
	std::string command = std::string(this->commandBuffer, this->bufferSize);
	ioUtils::write(this->forwardInputFd, command.data(), command.size());

	this->inputHistory.emplace_front(std::move(command));

	this->bufferSize = 0UL;
	// in case a command from history has been submitted reset move commandIndex as the most recent command 
	this->currentCommandIndex = -1L;

	getyx(this->inputWin, y, x);
	wmove(this->inputWin, y, 0);

	this->clearHints();
	this->writePromptLine();
}


OutputTab::OutputTab(std::string const& title, int32_t borderChar, int32_t colorPair, CurseWindow* parent) :
	BasicTab(borderChar, colorPair, parent),
	title{title}
{
	this->dispatcher[KEY_UP]        = [this] { this->scrollContentUp(); };
	this->dispatcher[KEY_DOWN]      = [this] { this->scrollContentDown(); };
	// this->dispatcher[KEY_MOUSE]     = [this] {
	// 	MEVENT event;
	// 	if (::getmouse(&event) == OK)
	// 	{
	// 		if (event.bstate & BUTTON4_PRESSED)			this->scrollContentUp();
	// 		else if (event.bstate & BUTTON5_PRESSED)	this->scrollContentDown();
	// 	}
	// };
}

OutputTab::OutputTab(OutputTab&& other) noexcept :
	BasicTab(std::move(other)),
	titleWin{other.titleWin},
	divLineWin{other.divLineWin},
	outputWin{other.titleWin},
	title{std::move(other.title)},
	state{std::move(other.state)},
	topLineScroll{other.topLineScroll}
{
	other.divLineWin = nullptr;
	other.titleWin = nullptr;
	other.outputWin = nullptr;
}

void OutputTab::refresh(void) const noexcept
{
	BasicTab::refresh();
	::wnoutrefresh(this->titleWin);
	::wnoutrefresh(this->divLineWin);
	::wnoutrefresh(this->outputWin);
}

void OutputTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	BasicTab::draw(h, w, y, x);
	if (this->borderWin != nullptr)		// if there's a border adjust position and size of the title
	{
		h -= 2;
		w -= 2;
		y += 1;
		x += 1;
	}

	if (this->title.empty() == false)
	{
		assert((w + 1) > static_cast<int32_t>(this->title.size()) and "Tab title longer than tab itself");
		// add the title
		this->titleWin = ::newwin(3, this->title.size() + 2, y, x + 1);
		if (this->titleWin == nullptr)
		{
			LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
			throw CliException("Failed to create window");
		}

		if (this->colorPair != -1)
			::wattron(this->titleWin, COLOR_PAIR(this->colorPair));
		::wattron(this->titleWin, A_BOLD);

		::wborder(this->titleWin, 0, 0, 0, 0, 0, 0, 0, 0);
		mvwaddstr(this->titleWin, 1, 1, this->title.data());

		if (this->colorPair != -1)
			::wattroff(this->titleWin, COLOR_PAIR(this->colorPair));
		::wattroff(this->titleWin, A_BOLD);

		// add a div line between title and ouput
		this->divLineWin = ::newwin(1, w - 3 - this->title.size() - 2, y + 2, x + 2 + this->title.size() + 2);
		if (this->colorPair != -1)
			::wattron(this->divLineWin, COLOR_PAIR(this->colorPair));
		::wattron(this->divLineWin, A_BOLD);

		mvwhline(this->divLineWin, 0, 0, ACS_HLINE, w);

		if (this->colorPair != -1)
			::wattroff(this->divLineWin, COLOR_PAIR(this->colorPair));
		::wattroff(this->divLineWin, A_BOLD);

		h -= 4, w -= 2;
		y += 4, x += 1;
	}

	this->outputWin = ::newwin(h, w, y, x);
	if (this->outputWin == nullptr)
	{
		LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
		throw CliException("Failed to create window");
	}
	::keypad(this->outputWin, true);
	::scrollok(this->outputWin, true);
	::idlok(this->outputWin, true);

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

		::wrefresh(this->divLineWin);
		::wclear(this->divLineWin);
		::delwin(this->divLineWin);
		this->divLineWin = nullptr;
	}

	::wrefresh(this->outputWin);
	::wclear(this->outputWin);
	::delwin(this->outputWin);
	this->outputWin = nullptr;
}

void OutputTab::activate(void)
{
	BasicTab::activate();
	curs_set(0);

	if (this->divLineWin == nullptr)
		return;

	int32_t _, widthDivLine;
	getmaxyx(this->divLineWin, _, widthDivLine);

	if (this->colorPair != -1)
		::wattron(this->divLineWin, COLOR_PAIR(this->colorPair));
	::wattron(this->divLineWin, A_BOLD | A_BLINK);

	mvwhline(this->divLineWin, 0, 0, ACS_HLINE, widthDivLine);

	if (this->colorPair != -1)
		::wattroff(this->divLineWin, COLOR_PAIR(this->colorPair));
	::wattroff(this->divLineWin, A_BOLD | A_BLINK);

	this->refresh();
}

void OutputTab::deactivate(void)
{
	BasicTab::deactivate();
	curs_set(1);

	if (this->divLineWin == nullptr)
		return;

	int32_t _, widthDivLine;
	getmaxyx(this->divLineWin, _, widthDivLine);

	if (this->colorPair != -1)
		::wattron(this->divLineWin, COLOR_PAIR(this->colorPair));
	::wattron(this->divLineWin, A_BOLD);

	mvwhline(this->divLineWin, 0, 0, ACS_HLINE, widthDivLine);

	if (this->colorPair != -1)
		::wattroff(this->divLineWin, COLOR_PAIR(this->colorPair));
	::wattroff(this->divLineWin, A_BOLD);

	this->refresh();
}

void OutputTab::appendContent(std::string const& newContent, TextAlign align)
{
	this->state.emplace_back(newContent, align);

	int32_t maxVerticalSpace, _;
	(void)_;
	getmaxyx(this->outputWin, maxVerticalSpace, _);

	if (static_cast<int32_t>(this->state.size()) > maxVerticalSpace)
	{
		// reached the end of the tab, rotate le lines and drop the oldest one
		::wscrl(this->outputWin, 1);
		::wmove(this->outputWin, maxVerticalSpace - 1, 0);
		::wclrtoeol(this->outputWin);
		this->topLineScroll += 1;
	}
	this->printLine(newContent, align);
}

void OutputTab::resize(int32_t h, int32_t w, int32_t y, int32_t x)
{
	int32_t oldMaxheight, newMaxheight, _, delta;
	(void)_;
	getmaxyx(this->outputWin, oldMaxheight, _);

	BasicTab::resize(h, w, y, x);
	getmaxyx(this->outputWin, newMaxheight, _);

	delta = newMaxheight - oldMaxheight;

	// if resize changed the vertical size, adjust the index of the first line shown
	if (delta > 0)		// vertical size increased
	{
		if (static_cast<int32_t>(this->topLineScroll) < delta)
			this->topLineScroll = 0;
		else
			this->topLineScroll -= delta;
	}
	else if (delta < 0)		// vertical size reduced
	{
		delta *= -1;
		if ((this->topLineScroll + delta) >= this->state.size())
			this->topLineScroll = this->state.size() - 1UL;
		else
			this->topLineScroll += delta;
	}

	for (size_t i = this->topLineScroll; i < this->state.size(); i++)
	{
		if (static_cast<int32_t>(i - this->topLineScroll) >= newMaxheight)
			break;
		this->printLine(this->state[i]);
	}
}

void OutputTab::printLine(std::string const& newContent, TextAlign align) const noexcept
{
	int32_t y, _;
	(void)_;
	getyx(this->outputWin, y, _);

	if (align != TextAlign::LEFT_ALIGN)
	{
		int32_t w;
		getmaxyx(this->outputWin, _, w);

		uint32_t lenWord = newContent.size();
		if (w > static_cast<int32_t>(lenWord))
		{
			uint32_t startText = 0U;
			if (align == TextAlign::MID_ALIGN)			startText = (w - lenWord) / 2U;
			else if (align == TextAlign::RIGHT_ALIGN)	startText = w - lenWord;

			::wmove(this->outputWin, y, startText);
		}
	}

	waddstr(this->outputWin, newContent.data());
	::wmove(this->outputWin, y + 1, 0);

	this->refresh();
}

void OutputTab::scrollContentUp(void) noexcept
{
	if (this->topLineScroll == 0)
		return;

	::wscrl(this->outputWin, -1);
	this->topLineScroll--;

	::wmove(this->outputWin, 0, 0);
	this->printLine(this->state[this->topLineScroll]);
	this->refresh();
}

void OutputTab::scrollContentDown(void) noexcept
{
	int32_t maxVerticalSpace, _;
	(void)_;
	getmaxyx(this->outputWin, maxVerticalSpace, _);

	if (maxVerticalSpace >= static_cast<int32_t>(this->state.size()))
		return;
	else if (this->topLineScroll + maxVerticalSpace == this->state.size())
		return;

	::wscrl(this->outputWin, 1);
	this->topLineScroll++;

	::wmove(this->outputWin, maxVerticalSpace - 1, 0);
	this->printLine(this->state[this->topLineScroll + maxVerticalSpace - 1]);
	this->refresh();
}


InOutTab::InOutTab(
	int32_t forwardInputFd,
	std::vector<std::string> const& hints,
	std::string const& prompt,
	std::string const& title,
	int32_t borderChar,
	int32_t colorPair,
	CurseWindow* parent
) :
	BasicTab(borderChar, colorPair, parent),
	InputTab(forwardInputFd, hints, prompt, borderChar, colorPair, parent),
	OutputTab(title, borderChar, colorPair, parent)
{
}

InOutTab::InOutTab(InOutTab&& other) noexcept :
	BasicTab(std::move(other)),
	InputTab(std::move(other)),
	OutputTab(std::move(other)),
	inputFrame{other.inputFrame}
{
	other.inputFrame = nullptr;
}

void InOutTab::refresh(void) const noexcept
{
	BasicTab::refresh();
	OutputTab::refresh();
	::wnoutrefresh(this->inputFrame);
	InputTab::refresh();
}

void InOutTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	BasicTab::draw(h, w, y, x);
	if (this->borderWin != nullptr)		// if there's a border adjust position and size of the title
	{
		h -= 2;
		w -= 2;
		y += 1;
		x += 1;
	}

	if (this->title.empty() == false)
	{
		assert((w + 1) > static_cast<int32_t>(this->title.size()) and "Tab title longer than tab itself");
		// title
		this->titleWin = ::newwin(3, this->title.size() + 2, y, x + 1);
		if (this->borderWin == nullptr)
		{
			LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
			throw CliException("Failed to create window");
		}

		if (this->colorPair != -1)
			::wattron(this->titleWin, COLOR_PAIR(this->colorPair));
		::wattron(this->titleWin, A_BOLD);

		::wborder(this->titleWin, 0, 0, 0, 0, 0, 0, 0, 0);
		mvwaddstr(this->titleWin, 1, 1, this->title.data());

		if (this->colorPair != -1)
			::wattroff(this->titleWin, COLOR_PAIR(this->colorPair));
		::wattroff(this->titleWin, A_BOLD);

		// add a div line between title and ouput
		this->divLineWin = ::newwin(1, w - 3 - this->title.size() - 2, y + 2, x + 2 + this->title.size() + 2);
		if (this->colorPair != -1)
			::wattron(this->divLineWin, COLOR_PAIR(this->colorPair));
		::wattron(this->divLineWin, A_BOLD);

		mvwhline(this->divLineWin, 0, 0, ACS_HLINE, w);

		if (this->colorPair != -1)
			::wattroff(this->divLineWin, COLOR_PAIR(this->colorPair));
		::wattroff(this->divLineWin, A_BOLD);

		h -= 4, w -= 2;
		y += 4, x += 1;
	}

	// output tab
	this->outputWin = ::newwin(h - 3, w, y, x);
	if (this->outputWin == nullptr)
	{
		LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
		throw CliException("Failed to create window");
	}
	::scrollok(this->outputWin, true);
	::idlok(this->outputWin, true);

	// frame input tab
	this->inputFrame = ::newwin(3, w, y + h - 3, x);
	if (this->inputFrame == nullptr)
	{
		LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
		throw CliException("Failed to create window");
	}
	if (this->colorPair != -1)
		::wattron(this->inputFrame, COLOR_PAIR(this->colorPair));
	::box(this->inputFrame, 0, 0);
	if (this->colorPair != -1)
		::wattroff(this->inputFrame, COLOR_PAIR(this->colorPair));

	// input tab
	this->inputWin = ::newwin(1, w - 2, y + h - 3 + 1, x + 1);
	if (this->inputWin == nullptr)
	{
		LOG_ERROR(LogContext::INTERFACE, "Failed to create window");
		throw CliException("Failed to create window");
	}

	::keypad(this->inputWin, true);
	this->writePromptLine();
}

void InOutTab::clear(void) noexcept
{
	BasicTab::clear();
	InputTab::clear();
	OutputTab::clear();

	::wrefresh(this->inputFrame);
	::wclear(this->inputFrame);
	::delwin(this->inputFrame);
	this->inputFrame = nullptr;
}

void InOutTab::activate(void)
{
	BasicTab::activate();
	OutputTab::activate();
	curs_set(1);			// Output hides the cursor when it activates
	InputTab::activate();
}

void InOutTab::deactivate(void)
{
	BasicTab::deactivate();
	OutputTab::deactivate();
	InputTab::deactivate();
}

void InOutTab::terminateInput(void)
{
	InputTab::terminateInput();

	this->state.emplace_back(this->inputHistory.front(), TextAlign::LEFT_ALIGN);
	// show last input
	this->appendContent(this->inputHistory.front());
}