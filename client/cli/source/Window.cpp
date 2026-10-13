#include "Window.hpp"
#include "BasicTab.hpp"
#include "Logger.hpp"
#include "UI.hpp"

#include <cassert>
#include <format>


Window::Window(UI* engine) noexcept : engine{engine}
{
	assert(engine != nullptr and "Null pointer for engine instance received");
}

Window::~Window(void) noexcept
{
}

void Window::clear(void) noexcept
{
	for (auto const& [_, tab] : this->tabs)
		tab->clear();
	this->refresh();
}

void Window::updateContentWindow(void) noexcept
{
	for (auto const& [_, tab] : this->tabs)
		tab->updateContent();
	this->getActiveTab()->refresh();
	this->refresh();
}

void Window::handleUserInput(void)
{
	this->getActiveTab()->handleUserInput();
	this->refresh();
}

void Window::switchActiveTab(int32_t index)
{
	if (this->tabs.empty() == true)
		return;

	this->getActiveTab()->deactivate();
	if (index > -1)		// switch to a selected tab
	{
		assert(index < static_cast<int32_t>(this->tabs.size()) and "index table to switch overflow");
		this->activeTabIndex = index;
	}
	else
		this->activeTabIndex = (this->activeTabIndex + 1) % this->tabs.size();

	// skip tabs if necessary
	while (this->tabsToSkip.count(this->activeTabIndex) != 0UL)
		this->activeTabIndex = (this->activeTabIndex + 1) % this->tabs.size();

	this->getActiveTab()->activate();
}

void Window::resize(int32_t height, int32_t width)
{
	this->clear();
	this->draw(height, width);

	// because resize is not handled by ncurses there might be some garbage to read, flush it
	::flushinp();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Window resized to h: {}, w: {}", this->height, this->width));
}
