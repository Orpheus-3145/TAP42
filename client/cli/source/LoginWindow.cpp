#include "LoginWindow.hpp"
#include "InOutTab.hpp"
#include "CLI.hpp"
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
	this->tabs[LoginWindow::USERNAME] = std::make_unique<InOutTab>(this->commandFd, std::vector<std::string>(), true, Config::PROMPT, "", 0, BLUE_COLOR, this);

	OutputTab* tab = dynamic_cast<OutputTab*>(this->tabs.at(LoginWindow::USERNAME).get());
	assert(tab != nullptr and "current tab doesn't support appending content");
	tab->appendContent(" ");
	tab->appendContent(" Enter username:", TextAlign::MID_ALIGN);

	this->tabsToSkip.insert(LoginWindow::FRAME);
	this->switchActiveTab(LoginWindow::USERNAME);
}

void LoginWindow::draw(int32_t height, int32_t width)
{
	this->height = (height % 4) != 0 ? ((height / 4) * 4) : height;		// has to be multiple of 4
	this->width = (width % 4) != 0 ? ((width / 4) * 4) : width;			// has to be multiple of 4

	this->tabs.at(LoginWindow::FRAME)->draw(
		this->height, 
		this->width,
		0,
		0
	);
	int32_t heightTabs = this->height / 4;
	int32_t widthTabs = this->width / 4;

	this->tabs.at(LoginWindow::USERNAME)->draw(
		heightTabs,
		widthTabs,
		(this->height - heightTabs) / 2,
		(this->width - widthTabs) / 2
	);

	this->updateContentWindow();
	this->refresh();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing login window, size h: {}, w: {}", this->height, this->width));
}
