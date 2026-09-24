#include "OutputTab.hpp"
#include "Exceptions.hpp"
#include "Logger.hpp"

#include <cassert>

OutputTab::OutputTab(std::string const& title, int32_t borderChar, int32_t colorPair, TapWindow* parent) :
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

	this->outputWin = ::newwin(h, w, y, x);
	if (this->outputWin == nullptr)
		throw AppException(ErrorCode::UI_INVALID_SIZE);

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
	if (this->outputWin == nullptr)
		return;

	int32_t maxVerticalSpace, _;
	(void)_;
	getmaxyx(this->outputWin, maxVerticalSpace, _);

	if (static_cast<int32_t>(this->state.size()) > maxVerticalSpace)
		this->topLineScroll += 1;
}

void OutputTab::resize(int32_t h, int32_t w, int32_t y, int32_t x)
{
	int32_t oldMaxheight, newMaxheight, _, delta;
	(void)_;
	getmaxyx(this->outputWin, oldMaxheight, _);

	BasicTab::resize(h, w, y, x);

	if (this->topLineScroll != 0UL)
	{
		// topLineScroll != 0 -> there are more lines of content than lines available
		// in the window, therefore a change in height (delta != 0) needs to adjust
		// the first index of the content line to print
		getmaxyx(this->outputWin, newMaxheight, _);
		delta = newMaxheight - oldMaxheight;
	
		if (delta > 0)			// vertical size increased
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
	}
	this->updateContent();
}

void OutputTab::updateContent(void) const noexcept
{
	wmove(this->outputWin, 0, 0);
	int32_t newMaxheight, _;
	(void)_;
	getmaxyx(this->outputWin, newMaxheight, _);

	int32_t nLinesToPrint = std::min(newMaxheight, static_cast<int32_t>(this->state.size() - this->topLineScroll));

	for (int32_t i = 0; i < nLinesToPrint; i++)
		this->printLine(this->state[this->topLineScroll + i]);
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
