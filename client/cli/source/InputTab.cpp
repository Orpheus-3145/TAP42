#include "InputTab.hpp"
#include "Exceptions.hpp"
#include "Utils.hpp"
#include "UI.hpp"


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
		this->commandBuffer[this->bufferSize++] = COMMAND_SP;
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

	this->writePromptLine();
}

void InputTab::deactivate(void)
{
	BasicTab::deactivate();

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
	if (this->bufferSize >= Config::CMD_BUFFER_SIZE)
	{
		if (this->bufferSize > Config::CMD_BUFFER_SIZE)
			return;

		waddch(this->inputWin, '-');
		this->bufferSize++;
		this->refresh();
	}

	int32_t y, x;
	bool resetCursorPos = false;

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

