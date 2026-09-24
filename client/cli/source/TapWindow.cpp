#include "TapWindow.hpp"
#include "BasicTab.hpp"
#include "Logger.hpp"

#include <cassert>
#include <format>


TapWindow::~TapWindow(void) noexcept
{
}

void TapWindow::clear(void) noexcept
{
	for (auto const& [_, tab] : this->tabs)
		tab->clear();
}

void TapWindow::updateContentWindow(void) const noexcept
{
	for (auto const& [_, tab] : this->tabs)
		tab->updateContent();
}

void TapWindow::handleUserInput(void)
{
	this->getActiveTab()->handleUserInput();
}

void TapWindow::switchActiveTab(int32_t index)
{
	if (this->tabs.empty() == true)
		return;

	this->getActiveTab()->deactivate();
	if (index > -1)		// switch to a selected tab
	{
		assert(index < static_cast<int32_t>(this->tabs.size()) and "index table to switch > number of tabs");
		this->activeTabIndex = index;
	}
	else
		this->activeTabIndex = (this->activeTabIndex + 1) % this->tabs.size();

	// skip tabs if necessary
	while (this->tabsToSkip.count(this->activeTabIndex) != 0UL)
		this->activeTabIndex = (this->activeTabIndex + 1) % this->tabs.size();

	this->getActiveTab()->activate();
}

void TapWindow::resize(int32_t height, int32_t width)
{
	this->clear();
	this->draw(height, width);

	// because resize is not handled by ncurses there might be some garbage to read, flush it
	::flushinp();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Window resized to h: {}, w: {}", this->height, this->width));
}
