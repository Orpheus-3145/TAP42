#include "LoginWindow.hpp"
#include "CLI.hpp"
#include "OutputTab.hpp"
#include "InputTab.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


LoginWindow::LoginWindow(int32_t commandFd) :
	TapWindow(),
	commandFd{commandFd}
{
	assert(this->commandFd != -1 and "Invalid fd provided for forwarding username");

	this->tabs[LoginWindow::FRAME] = std::make_unique<BasicTab>(-1, BLUE_COLOR, this);
	this->tabs[LoginWindow::INFO] = std::make_unique<OutputTab>(0, BLUE_COLOR, this);
	this->tabs[LoginWindow::USERNAME] = std::make_unique<InputTab>(this->commandFd, std::vector<std::string>(), Config::PROMPT, 0, BLUE_COLOR, this);

	OutputTab* tab = dynamic_cast<OutputTab*>(this->tabs.at(LoginWindow::INFO).get());
	assert(tab != nullptr and "current tab doesn't support appending content");
	tab->appendContent(" ");
	tab->appendContent(" Enter username:", false, TextAlign::MID_ALIGN);

	this->tabsToSkip.insert(LoginWindow::FRAME);
	this->tabsToSkip.insert(LoginWindow::INFO);
	this->switchActiveTab(LoginWindow::USERNAME);
}

void LoginWindow::draw(int32_t height, int32_t width)
{
	this->height = (height % 3) != 0 ? ((height / 3) * 3) : height;		// has to be multiple of 3
	this->width = (width % 4) != 0 ? ((width / 4) * 4) : width;			// has to be multiple of 4

	this->tabs.at(LoginWindow::FRAME)->draw(
		this->height, 
		this->width,
		0,
		0
	);
	int32_t heightInfoTab = this->height / 3;
	int32_t widthTabs = this->width / 4;

	this->tabs.at(LoginWindow::INFO)->draw(
		heightInfoTab,
		widthTabs,
		(this->height - heightInfoTab) / 2,
		(this->width - widthTabs) / 2
	);

	this->tabs.at(LoginWindow::USERNAME)->draw(
		3,
		widthTabs - 2,
		(this->height - heightInfoTab) / 2 + heightInfoTab - 4,
		(this->width - widthTabs) / 2 + 1
	);

	this->updateContentWindow();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing login window, size h: {}, w: {}", this->height, this->width));
}
