#include "BasicTab.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


BasicTab::BasicTab(int32_t borderChar, int32_t colorPair, TapWindow* parent) : 
	borderChar{borderChar},
	colorPair{colorPair},
	parent{parent}
{
	if (::has_colors() == false)
		this->colorPair = -1;

	this->dispatcher['\t'] = [this] { if (this->parent) this->parent->switchActiveTab(); };
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
	this->borderWin = ::newwin(h, w, y, x);
	if (this->borderWin == nullptr)
		throw AppException(ErrorCode::UI_INVALID_SIZE);

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

void BasicTab::clear(void) noexcept
{
	::wclear(this->borderWin);
	::wrefresh(this->borderWin);
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

	auto it = this->dispatcher.find(inputChar);
	if (it != this->dispatcher.end())
		it->second();
}
