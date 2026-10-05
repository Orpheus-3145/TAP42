#include "LoginWindow.hpp"
#include "CLI.hpp"
#include "OutputTab.hpp"
#include "InputTab.hpp"
#include "ButtonTab.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


LoginWindow::LoginWindow(int32_t commandFd, UI* engine) :
	TapWindow(engine),
	commandFd{commandFd}
{
	assert(this->commandFd != -1 and "Invalid fd provided for forwarding username");

	this->tabs[LoginWindow::FRAME] = std::make_unique<BasicTab>(-1, BLUE_COLOR, this);
	this->tabs[LoginWindow::INFO] = std::make_unique<OutputTab>(0, BLUE_COLOR, this);
	this->tabs[LoginWindow::USERNAME] = std::make_unique<InputTab>(this->commandFd, std::vector<std::string>(), Config::PROMPT, 0, BLUE_COLOR, this);
	this->tabs[LoginWindow::CREATE_PLAYER] = std::make_unique<ButtonTab>(
		"CREATE NEW",
		[this]
		{
			this->switchActiveTab(LoginWindow::USERNAME);
			this->engine->switchWindow(GamePhase::PLAYER_CREATE);
		},
		GREEN_COLOR,
		this
	);
	this->tabs[LoginWindow::CLOSE] = std::make_unique<ButtonTab>("CLOSE", [this] { this->engine->stop(); }, RED_COLOR, this);

	this->tabsToSkip.insert(LoginWindow::FRAME);
	this->tabsToSkip.insert(LoginWindow::INFO);
	this->switchActiveTab(LoginWindow::USERNAME);
}

void LoginWindow::draw(int32_t height, int32_t width)
{
	this->height = (height % 3) != 0 ? ((height / 3) * 3) : height;
	this->width = (width % 4) != 0 ? ((width / 4) * 4) : width;

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

	OutputTab* tab = dynamic_cast<OutputTab*>(this->tabs.at(LoginWindow::INFO).get());
	assert(tab != nullptr and "current tab doesn't support appending content");
	tab->clearContent();
	tab->appendContent(" ", TextAlign::LEFT, A_NORMAL, false);
	tab->appendContent(" Enter username:", TextAlign::MID, A_BOLD, false);


	this->tabs.at(LoginWindow::USERNAME)->draw(
		3,
		widthTabs - 4,
		(this->height - heightInfoTab) / 2 + heightInfoTab - 4 - 3,
		(this->width - widthTabs) / 2 + 2
	);

	int32_t starty, startxAction1, startxAction2;
	ButtonTab* btnTab = dynamic_cast<ButtonTab*>(this->tabs.at(LoginWindow::CREATE_PLAYER).get());
	assert(btnTab != nullptr and "current tab doesn't support getWidth()");

	starty = (this->height - heightInfoTab) / 2 + heightInfoTab - 3 - 1;
	startxAction1 = (this->width - widthTabs) / 2 + 2;
	startxAction2 = startxAction1 + widthTabs - btnTab->getWidth() - 4;

	this->tabs.at(LoginWindow::CREATE_PLAYER)->draw(
		1,
		1,
		starty,
		startxAction1
	);

	btnTab = dynamic_cast<ButtonTab*>(this->tabs.at(LoginWindow::CLOSE).get());
	assert(btnTab != nullptr and "current tab doesn't support getWidth()");

	startxAction2 = startxAction1 + widthTabs - btnTab->getWidth() - 4;
	this->tabs.at(LoginWindow::CLOSE)->draw(
		1,
		1,
		starty,
		startxAction2
	);

	this->updateContentWindow();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing login window, size h: {}, w: {}", this->height, this->width));
}
