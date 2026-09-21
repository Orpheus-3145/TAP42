#include "CurseWindow.hpp"
#include "BasicTab.hpp"

#include <cassert>


CurseWindow::~CurseWindow(void) noexcept
{
}

void CurseWindow::clear(void) noexcept
{
	for (std::unique_ptr<BasicTab>& tab : this->tabs)
		tab.reset();
}

void CurseWindow::handleUserInput(void)
{
	this->getActiveTab()->handleUserInput();
}

void CurseWindow::switchActiveTab(int32_t index)
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
