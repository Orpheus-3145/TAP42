#include "PlayerCreateWindow.hpp"
#include "OutputTab.hpp"
#include "InputTab.hpp"
#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


PlayerCreateWindow::PlayerCreateWindow(int32_t commandFd) :
	TapWindow(),
	commandFd{commandFd}
{
	assert(this->commandFd != -1 and "Invalid fd provided for forwarding username");

	this->tabs[PlayerCreateWindow::FRAME] = std::make_unique<BasicTab>(-1, GREEN_COLOR, this);
	this->tabs[PlayerCreateWindow::INFO] = std::make_unique<OutputTab>(0, GREEN_COLOR, this);
	this->tabs[PlayerCreateWindow::USERNAME] = std::make_unique<InputTab>(this->commandFd, std::vector<std::string>(), Config::PROMPT, 0, GREEN_COLOR, this);

	OutputTab* tab = dynamic_cast<OutputTab*>(this->tabs.at(PlayerCreateWindow::INFO).get());
	assert(tab != nullptr and "current tab doesn't support appending content");
	tab->appendContent(" ");
	tab->appendContent(" Enter username:", false, TextAlign::MID_ALIGN);
	tab->appendContent(" ");
	tab->appendContent(" ");
	tab->appendContent(" ");
	tab->appendContent(" ");
	tab->appendContent("<TBD MORE STUFF TO ADD>", false, TextAlign::MID_ALIGN);

	this->tabsToSkip.insert(PlayerCreateWindow::FRAME);
	this->tabsToSkip.insert(PlayerCreateWindow::INFO);
	this->switchActiveTab(PlayerCreateWindow::USERNAME);
}

void PlayerCreateWindow::draw(int32_t height, int32_t width)
{
	this->height = (height % 2) != 0 ? ((height / 2) * 2) : height;		// has to be multiple of 2
	this->width = (width % 4) != 0 ? ((width / 4) * 4) : width;			// has to be multiple of 4

	this->tabs.at(PlayerCreateWindow::FRAME)->draw(
		this->height, 
		this->width,
		0,
		0
	);
	int32_t heightInfoTab = this->height / 2;
	int32_t widthTabs = this->width / 4;

	this->tabs.at(PlayerCreateWindow::INFO)->draw(
		heightInfoTab,
		widthTabs,
		(this->height - heightInfoTab) / 2,
		(this->width - widthTabs) / 2
	);

	this->tabs.at(PlayerCreateWindow::USERNAME)->draw(
		3,
		widthTabs - 2,
		(this->height - heightInfoTab) / 2 + heightInfoTab - 4,
		(this->width - widthTabs) / 2 + 1
	);

	this->updateContentWindow();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing create player window, size h: {}, w: {}", this->height, this->width));
}
