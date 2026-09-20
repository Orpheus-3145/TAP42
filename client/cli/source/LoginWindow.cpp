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

	this->tmp = std::make_unique<BasicTab>(-1, BLUE_COLOR, this);
	this->frame = std::make_unique<BasicTab>(0, BLUE_COLOR, this);
	this->inputNameTab = std::make_unique<SingleInputTab>(this->commandFd, std::vector<std::string>(), Config::PROMPT, 0, BLUE_COLOR, this);
	this->outputNameTab = std::make_unique<OutputTab>("", -1, BLUE_COLOR, this);
}

void LoginWindow::draw(int32_t height, int32_t width)
{
	this->height = (height % 4) != 0 ? ((height / 4) * 4) : height;		// has to be an even number
	this->width = (width % 4) != 0 ? ((width / 4) * 4) : width;			// has to be an even number

	this->tmp->draw(
		this->height, 
		this->width,
		0,
		0
	);
	int32_t widthTabs = this->width / 4;
	int32_t heightTabs = this->height / 4;

	this->frame->draw(
		heightTabs,
		widthTabs,
		(this->height - heightTabs) / 2,
		(this->width - widthTabs) / 2
	);

	this->outputNameTab->draw(
		heightTabs - 2 - 3,
		widthTabs - 2,
		(this->height - heightTabs) / 2 + 1,
		(this->width - widthTabs) / 2 + 1
	);

	this->outputNameTab->appendContent(" ");
	this->outputNameTab->appendContent(" Insert user name:", TextAlign::MID_ALIGN);

	this->inputNameTab->draw(
		3,
		widthTabs - 2,
		(heightTabs - 2 - 3) + (this->height - heightTabs) / 2 + 1,
		(this->width - widthTabs) / 2 + 1
	);

	this->currentTab = this->inputNameTab.get();
	this->refresh();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing login window, size h: {}, w: {}", this->height, this->width));
}

void LoginWindow::clear(void) noexcept
{
	this->frame.reset();
	this->outputNameTab.reset();
	this->inputNameTab.reset();
}

void LoginWindow::resize(int32_t height, int32_t width)
{
	this->height = (height % 4) != 0 ? ((height / 4) * 4) : height;		// has to be an even number
	this->width = (width % 4) != 0 ? ((width / 4) * 4) : width;			// has to be an even number

	::resizeterm(this->height, this->width);

	this->tmp->resize(
		this->height, 
		this->width,
		0,
		0
	);

	int32_t widthTabs = this->width / 4;
	int32_t heightTabs = this->height / 4;

	this->frame->resize(
		heightTabs,
		widthTabs,
		(this->height - heightTabs) / 2,
		(this->width - widthTabs) / 2
	);

	this->outputNameTab->resize(
		heightTabs - 2 - 3,
		widthTabs - 2,
		(this->height - heightTabs) / 2 + 1,
		(this->width - widthTabs) / 2 + 1
	);

	this->inputNameTab->resize(
		3,
		widthTabs - 2,
		(heightTabs - 2 - 3) + (this->height - heightTabs) / 2 + 1,
		(this->width - widthTabs) / 2 + 1
	);

	// because resize is not handled by ncurses there might be some garbage to read, flush it
	::flushinp();
	
	this->refresh();
	LOG_DEBUG(LogContext::INTERFACE, std::format("Resized login window to h: {}, w: {}", this->height, this->width));
}

void LoginWindow::readInput(void)
{
	InputTab* inputTab = dynamic_cast<InputTab*>(this->currentTab);
	assert(inputTab != nullptr and "current input doesn't support handling input");

	inputTab->handleUserInput();
}
