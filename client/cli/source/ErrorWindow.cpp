#include "ErrorWindow.hpp"
#include "OutputTab.hpp"
#include "ButtonTab.hpp"
#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


ErrorWindow::ErrorWindow(void) : TapWindow()
{
	this->tabs[ErrorWindow::FRAME] = std::make_unique<BasicTab>(-1, RED_COLOR, this);
	this->tabs[ErrorWindow::INFO] = std::make_unique<OutputTab>("ERROR", 0, RED_COLOR, this);

	this->tabsToSkip.insert(ErrorWindow::FRAME);
	this->tabsToSkip.insert(ErrorWindow::INFO);
}

void ErrorWindow::draw(int32_t height, int32_t width)
{
	assert(this->tabs.find(ACTION1) != this->tabs.end() and "error window needs at least one callback set");

	this->height = (height % 2) == 0 ? height : height - 1;		// has to be even
	this->width = width;

	this->tabs.at(ErrorWindow::FRAME)->draw(
		this->height, 
		this->width,
		0,
		0
	);
	int32_t heightTabs = this->height / 2;
	int32_t widthTabs = this->width / 2;

	this->tabs.at(ErrorWindow::INFO)->draw(
		heightTabs,
		widthTabs,
		(this->height - heightTabs) / 2,
		(this->width - widthTabs) / 2
	);

	int32_t starty, startxAction1, startxAction2;
	ButtonTab* btnTab = dynamic_cast<ButtonTab*>(this->tabs.at(ErrorWindow::ACTION1).get());
	assert(btnTab != nullptr and "current tab doesn't support getWidth()");

	starty = (this->height - heightTabs) / 2 + heightTabs - 3 - 1;
	if (this->tabs.find(ErrorWindow::ACTION2) != this->tabs.end())
		startxAction1 = (this->width - widthTabs) / 2 + 2;
	else
		startxAction1 = (this->width - btnTab->getWidth()) / 2;

	this->tabs.at(ErrorWindow::ACTION1)->draw(
		1,
		1,
		starty,
		startxAction1
	);
	if (this->tabs.find(ErrorWindow::ACTION2) != this->tabs.end())
	{
		btnTab = dynamic_cast<ButtonTab*>(this->tabs.at(ErrorWindow::ACTION2).get());
		assert(btnTab != nullptr and "current tab doesn't support getWidth()");

		startxAction2 = startxAction1 + widthTabs - btnTab->getWidth() - 4;
		this->tabs.at(ErrorWindow::ACTION2)->draw(
			1,
			1,
			starty,
			startxAction2
		);
	}

	this->updateContentWindow();
	this->refresh();
	curs_set(0);

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing error window, size h: {}, w: {}", this->height, this->width));
}

void ErrorWindow::clear(void) noexcept
{
	TapWindow::clear();

	this->tabs.erase(ErrorWindow::ACTION1);
	this->tabs.erase(ErrorWindow::ACTION2);

	OutputTab* descTab = dynamic_cast<OutputTab*>(this->tabs.at(ErrorWindow::INFO).get());
	assert(descTab != nullptr and "current tab doesn't support removing content");
	descTab->clearContent();

	curs_set(1);
}

void ErrorWindow::updateDescription(std::string const& description)
{
	OutputTab* descTab = dynamic_cast<OutputTab*>(this->tabs.at(ErrorWindow::INFO).get());
	assert(descTab != nullptr and "current tab doesn't support appending content");

	descTab->appendContent(" ");
	descTab->appendContent(description, false, TextAlign::MID_ALIGN);
}

void ErrorWindow::setAction1(std::string const& actionName, std::function<void()> action)
{
	this->tabs[ErrorWindow::ACTION1] = std::make_unique<ButtonTab>(actionName, action, -1, this);
	this->switchActiveTab(ErrorWindow::ACTION1);
}

void ErrorWindow::setAction2(std::string const& actionName, std::function<void()> action)
{
	this->tabs[ErrorWindow::ACTION2] = std::make_unique<ButtonTab>(actionName, action, -1, this);
}
