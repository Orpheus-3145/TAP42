#include "InOutTab.hpp"
#include "Exceptions.hpp"

#include <cassert>


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
			throw AppException(ErrorCode::UI_INVALID_SIZE);

		if (this->colorPair != -1)
			::wattron(this->titleWin, COLOR_PAIR(this->colorPair));
		::wattron(this->titleWin, A_BOLD);

		::box(this->titleWin, 0, 0);
		mvwaddstr(this->titleWin, 1, 1, this->title.data());

		if (this->colorPair != -1)
			::wattroff(this->titleWin, COLOR_PAIR(this->colorPair));
		::wattroff(this->titleWin, A_BOLD);

		// add a div line between title and ouput
		this->divLineWin = ::newwin(1, w - 3 - this->title.size() - 2, y + 2, x + 2 + this->title.size() + 2);
		if (this->divLineWin == nullptr)
			throw AppException(ErrorCode::UI_INVALID_SIZE);

		if (this->colorPair != -1)
			::wattron(this->divLineWin, COLOR_PAIR(this->colorPair));
		if (this->isActive == true)
			::wattron(this->divLineWin, A_BLINK);
		::wattron(this->divLineWin, A_BOLD);

		mvwhline(this->divLineWin, 0, 0, ACS_HLINE, w);

		if (this->colorPair != -1)
			::wattroff(this->divLineWin, COLOR_PAIR(this->colorPair));
		if (this->isActive == true)
			::wattroff(this->divLineWin, A_BLINK);
		::wattroff(this->divLineWin, A_BOLD);

		h -= 4, w -= 2;
		y += 4, x += 1;
	}

	// output tab
	this->outputWin = ::newwin(h - 3, w, y, x);
	if (this->outputWin == nullptr)
		throw AppException(ErrorCode::UI_INVALID_SIZE);

	::scrollok(this->outputWin, true);
	::idlok(this->outputWin, true);

	// frame input tab
	this->inputFrame = ::newwin(3, w, y + h - 3, x);
	if (this->inputFrame == nullptr)
		throw AppException(ErrorCode::UI_INVALID_SIZE);

	if (this->colorPair != -1)
		::wattron(this->inputFrame, COLOR_PAIR(this->colorPair));
	::box(this->inputFrame, 0, 0);
	if (this->colorPair != -1)
		::wattroff(this->inputFrame, COLOR_PAIR(this->colorPair));

	// input tab
	this->inputWin = ::newwin(1, w - 2, y + h - 3 + 1, x + 1);
	if (this->inputWin == nullptr)
		throw AppException(ErrorCode::UI_INVALID_SIZE);

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

	// show last input
	this->appendContent(this->prompt + this->inputHistory.front());
}