#include "LoginWindow.hpp"
#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


LoginWindow::LoginWindow(int32_t commandFd) :
	CurseWindow(),
	commandFd{commandFd}
{
	assert(this->commandFd != -1 and "Invalid fd provided for forwarding username");

	this->frame = std::make_unique<BasicTab>(-1, BLUE_COLOR, this);
	this->usernameTab = std::make_unique<InOutTab>(this->commandFd, std::vector<std::string>(), Config::PROMPT, "", 0, BLUE_COLOR, this);
}

void LoginWindow::draw(int32_t height, int32_t width)
{
	this->height = (height % 4) != 0 ? ((height / 4) * 4) : height;		// has to be multiple of 4
	this->width = (width % 4) != 0 ? ((width / 4) * 4) : width;			// has to be multiple of 4

	this->frame->draw(
		this->height, 
		this->width,
		0,
		0
	);
	int32_t heightTabs = this->height / 4;
	int32_t widthTabs = this->width / 4;

	this->usernameTab->draw(
		heightTabs,
		widthTabs,
		(this->height - heightTabs) / 2,
		(this->width - widthTabs) / 2
	);
	this->usernameTab->appendContent(" ");
	this->usernameTab->appendContent(" Insert user name:", TextAlign::MID_ALIGN);

	this->currentTab = this->usernameTab.get();
	this->refresh();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing login window, size h: {}, w: {}", this->height, this->width));
}

void LoginWindow::clear(void) noexcept
{
	this->frame.reset();
	this->usernameTab.reset();
}

void LoginWindow::resize(int32_t height, int32_t width)
{
	this->height = (height % 4) != 0 ? ((height / 4) * 4) : height;		// has to be multiple of 4
	this->width = (width % 4) != 0 ? ((width / 4) * 4) : width;			// has to be multiple of 4

	::resizeterm(this->height, this->width);

	this->frame->draw(
		this->height, 
		this->width,
		0,
		0
	);
	int32_t heightTabs = this->height / 4;
	int32_t widthTabs = this->width / 4;

	this->usernameTab->draw(
		heightTabs,
		widthTabs,
		(this->height - heightTabs) / 2,
		(this->width - widthTabs) / 2
	);

	// because resize is not handled by ncurses there might be some garbage to read, flush it
	::flushinp();
	
	this->refresh();
	LOG_DEBUG(LogContext::INTERFACE, std::format("Resized login window to h: {}, w: {}", this->height, this->width));
}

void LoginWindow::readInput(void)
{
	this->usernameTab->handleUserInput();
}
