#include "PlayerCreateWindow.hpp"
#include "OutputTab.hpp"
#include "InputTab.hpp"
#include "ButtonTab.hpp"
#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


PlayerCreateWindow::PlayerCreateWindow(int32_t commandFd, UI* engine) :
	TapWindow(engine),
	commandFd{commandFd}
{
	assert(this->commandFd != -1 and "Invalid fd provided for forwarding username");

	this->tabs[PlayerCreateWindow::FRAME] = std::make_unique<BasicTab>(-1, GREEN_COLOR, this);
	this->tabs[PlayerCreateWindow::INFO] = std::make_unique<OutputTab>(0, GREEN_COLOR, this);
	this->tabs[PlayerCreateWindow::USERNAME] = std::make_unique<InputTab>(this->commandFd, std::vector<std::string>(), Config::PROMPT, 0, GREEN_COLOR, this);
	this->tabs[PlayerCreateWindow::BACK] = std::make_unique<ButtonTab>(
		"LOGIN",
		[this]
		{
			this->switchActiveTab(PlayerCreateWindow::USERNAME);
			this->engine->switchWindow(GamePhase::LOGIN);
		},
		BLUE_COLOR,
		this);
	this->tabs[PlayerCreateWindow::CLOSE] = std::make_unique<ButtonTab>("CLOSE", [this] { this->engine->stop(); }, RED_COLOR, this);

	this->tabsToSkip.insert(PlayerCreateWindow::FRAME);
	this->tabsToSkip.insert(PlayerCreateWindow::INFO);
	this->switchActiveTab(PlayerCreateWindow::USERNAME);
}

void PlayerCreateWindow::draw(int32_t height, int32_t width)
{
	this->height = (height % 2) != 0 ? ((height / 2) * 2) : height;
	this->width = (width % 4) != 0 ? ((width / 4) * 4) : width;

	this->tabs.at(PlayerCreateWindow::FRAME)->draw(
		this->height, 
		this->width,
		0,
		0
	);
	int32_t heightInfoTab = this->height / 2;
	int32_t widthTabs = this->width / 4;

	OutputTab* tab = dynamic_cast<OutputTab*>(this->tabs.at(PlayerCreateWindow::INFO).get());
	assert(tab != nullptr and "current tab doesn't support appending content");
	tab->clearContent();
	tab->appendContent(" ", TextAlign::LEFT, A_NORMAL, false);
	tab->appendContent(" Enter username:", TextAlign::MID, A_BOLD, false);
	this->tabs.at(PlayerCreateWindow::INFO)->draw(
		heightInfoTab,
		widthTabs,
		(this->height - heightInfoTab) / 2,
		(this->width - widthTabs) / 2
	);

	this->tabs.at(PlayerCreateWindow::USERNAME)->draw(
		3,
		widthTabs - 4,
		(this->height - heightInfoTab) / 2 + heightInfoTab - 4 - 3,
		(this->width - widthTabs) / 2 + 2
	);

	int32_t starty, startxAction1, startxAction2;
	ButtonTab* btnTab = dynamic_cast<ButtonTab*>(this->tabs.at(PlayerCreateWindow::BACK).get());
	assert(btnTab != nullptr and "current tab doesn't support getWidth()");

	starty = (this->height - heightInfoTab) / 2 + heightInfoTab - 3 - 1;
	startxAction1 = (this->width - widthTabs) / 2 + 2;
	startxAction2 = startxAction1 + widthTabs - btnTab->getWidth() - 4;

	this->tabs.at(PlayerCreateWindow::BACK)->draw(
		1,
		1,
		starty,
		startxAction1
	);

	btnTab = dynamic_cast<ButtonTab*>(this->tabs.at(PlayerCreateWindow::CLOSE).get());
	assert(btnTab != nullptr and "current tab doesn't support getWidth()");

	startxAction2 = startxAction1 + widthTabs - btnTab->getWidth() - 4;
	this->tabs.at(PlayerCreateWindow::CLOSE)->draw(
		1,
		1,
		starty,
		startxAction2
	);

	this->updateContentWindow();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing create player window, size h: {}, w: {}", this->height, this->width));
}
