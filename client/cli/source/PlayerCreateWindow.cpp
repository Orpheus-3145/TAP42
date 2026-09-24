#include "PlayerCreateWindow.hpp"
#include "InOutTab.hpp"
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
	this->tabs[PlayerCreateWindow::USERNAME] = std::make_unique<InOutTab>(this->commandFd, std::vector<std::string>(), true, Config::PROMPT, "", 0, GREEN_COLOR, this);

	OutputTab* tab = dynamic_cast<OutputTab*>(this->tabs.at(PlayerCreateWindow::USERNAME).get());
	assert(tab != nullptr and "current tab doesn't support appending content");
	tab->appendContent(" ");
	tab->appendContent(" Enter new username:", TextAlign::MID_ALIGN);

	this->tabsToSkip.insert(PlayerCreateWindow::FRAME);
	this->switchActiveTab(PlayerCreateWindow::USERNAME);
}

void PlayerCreateWindow::draw(int32_t height, int32_t width)
{
	this->height = (height % 4) != 0 ? ((height / 4) * 4) : height;		// has to be multiple of 4
	this->width = (width % 2) != 0 ? ((width / 2) * 2) : width;			// has to be an even number

	this->tabs.at(PlayerCreateWindow::FRAME)->draw(
		this->height, 
		this->width,
		0,
		0
	);

	int32_t heightTabs = this->height / 2;
	int32_t widthTabs = this->width / 4;

	this->tabs.at(PlayerCreateWindow::USERNAME)->draw(
		heightTabs,
		widthTabs,
		(this->height - heightTabs) / 2,
		(this->width - widthTabs) / 2
	);

	this->updateContentWindow();
	this->refresh();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing create player window, size h: {}, w: {}", this->height, this->width));
}
