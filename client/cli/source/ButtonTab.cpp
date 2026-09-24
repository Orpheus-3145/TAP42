#include "ButtonTab.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "UI.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


ButtonTab::ButtonTab(std::string const& content, std::function<void()> action, int32_t colorPair, TapWindow* parent) :
	BasicTab(-1, colorPair, parent),
	content{content},
	action{action}
{
	this->dispatcher[COMMAND_TERM] = [this] { this->action(); };
}

ButtonTab::ButtonTab(ButtonTab&& other) noexcept :
	BasicTab(std::move(other)),
	content{other.content},
	action{other.action},
	btnWin{other.btnWin}
{
	other.btnWin = nullptr;
}

void ButtonTab::draw(int32_t h, int32_t w, int32_t y, int32_t x)
{
	(void) h;
	(void) w;
	BasicTab::draw(3, this->content.size() + 2, y, x);

	this->borderWin = ::newwin(3, this->content.size() + 2, y, x);
	if (this->borderWin == nullptr)
		throw AppException(ErrorCode::UI_INVALID_SIZE);

	if (this->colorPair != -1)
		::wattron(this->borderWin, COLOR_PAIR(this->colorPair));
	::wattron(this->borderWin, A_BOLD);

	::box(this->borderWin, 0, 0);

	if (this->isActive)
		::wattroff(this->borderWin, A_BLINK);
	::wattroff(this->borderWin, A_BOLD);

	this->btnWin = ::newwin(1, this->content.size(), y + 1, x + 1);
	if (this->btnWin == nullptr)
		throw AppException(ErrorCode::UI_INVALID_SIZE);

	if (this->colorPair != -1)
		::wattron(this->btnWin, COLOR_PAIR(this->colorPair));
	if (this->isActive)
		::wattron(this->btnWin, A_BLINK);
	::wattron(this->btnWin, A_BOLD | A_UNDERLINE);

	::waddstr(this->btnWin, this->content.data());

	if (this->colorPair != -1)
		::wattroff(this->btnWin, COLOR_PAIR(this->colorPair));
	if (this->isActive)
		::wattroff(this->btnWin, A_BLINK);
	::wattroff(this->btnWin, A_BOLD | A_UNDERLINE);

	::keypad(this->borderWin, true);
	::keypad(this->btnWin, true);
	this->refresh();
}

void ButtonTab::refresh(void) const noexcept
{
	BasicTab::refresh();
	::wnoutrefresh(this->btnWin);
}

void ButtonTab::clear(void) noexcept
{
	BasicTab::clear();

	::wrefresh(this->btnWin);
	::wclear(this->btnWin);
	::delwin(this->btnWin);
	this->btnWin = nullptr;
}

void ButtonTab::activate(void)
{
	BasicTab::activate();

	if (this->colorPair != -1)
		::wattron(this->btnWin, COLOR_PAIR(this->colorPair));
	::wattron(this->btnWin, A_BOLD | A_UNDERLINE | A_BLINK);

	::wmove(this->btnWin, 0, 0);
	::wclrtoeol(this->btnWin);
	::waddstr(this->btnWin, this->content.data());

	if (this->colorPair != -1)
		::wattroff(this->btnWin, COLOR_PAIR(this->colorPair));
	::wattroff(this->btnWin, A_BOLD | A_UNDERLINE | A_BLINK);

	this->refresh();
}

void ButtonTab::deactivate(void)
{
	BasicTab::deactivate();

	if (this->colorPair != -1)
		::wattron(this->btnWin, COLOR_PAIR(this->colorPair));
	::wattron(this->btnWin, A_BOLD | A_UNDERLINE);

	::wmove(this->btnWin, 0, 0);
	::wclrtoeol(this->btnWin);
	::waddstr(this->btnWin, this->content.data());

	if (this->colorPair != -1)
		::wattroff(this->btnWin, COLOR_PAIR(this->colorPair));
	::wattroff(this->btnWin, A_BOLD | A_UNDERLINE);

	this->refresh();
}
