#include "GameWindow.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <format>
#include <cassert>


GameWindow::GameWindow(int32_t height, int32_t width, int32_t commandFd, int32_t chatFd) :
	height{height},
	width{width},
	commandFd{commandFd},
	chatFd{chatFd}
{
	assert(this->commandFd != -1 and "Invalid fd provided for forwarding commands");
	assert(this->chatFd != -1 and "Invalid fd provided for forwarding chat messages");

	this->show();
	this->refresh();
}
		
void GameWindow::show(void)
{
	int32_t height = (this->height % 2) == 0 ? this->height : this->height - 1;
	int32_t width = (this->width % 2) != 0 ? this->width : this->width - 1;
	int32_t starty = 0;
	int32_t startx = 0;
	this->frame = std::make_unique<OutputTab>(height, width, starty, startx, 0);

	height -= 2;
	width = (width - 5) / 2;
	startx += 2;
	starty += 1;
	this->commandTab = std::make_unique<InputTab>(height, width, starty, startx, this->commandFd, 0);
	this->commandTab->appendContent("This is where the input is shown");

	height /= 2;
	startx += width + 1;
	this->responseTab = std::make_unique<OutputTab>(height, width, starty, startx, 0);
	this->responseTab->appendContent("This is where responses are shown");

	starty += height;
	this->eventTab = std::make_unique<OutputTab>(height, width, starty, startx, 0);
	this->eventTab->appendContent("This is where events are shown");
	this->commandTab->refresh();

	LOG_DEBUG(LogContext::INTERFACE, std::format("CLI window size h: {}, w: {}", height, width));
}

void GameWindow::clear(void) noexcept
{
	this->frame.reset();
	this->commandTab.reset();
	this->responseTab.reset();
	this->eventTab.reset();
}

void GameWindow::readInput(void)
{
	this->commandTab->handleUserInput();
}

void GameWindow::resize(int32_t height, int32_t width)
{
	this->height = height;
	this->width = width;

	height = (height % 2) == 0 ? height : height - 1;
	width = (width % 2) != 0 ? width : width - 1;
	int32_t starty = 0;
	int32_t startx = 0;
	::resizeterm(height, width);
	this->frame->resize(height, width, starty, startx);

	height -= 2;
	width = (width - 5) / 2;
	startx += 2;
	starty += 1;
	this->commandTab->resize(height, width, starty, startx);

	height /= 2;
	startx += width + 1;
	this->responseTab->resize(height, width, starty, startx);

	starty += height;
	this->eventTab->resize(height, width, starty, startx);
	this->commandTab->refresh();

	// because resize is not handled by ncurses there might be some garbage to read, flush it
	::flushinp();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Window resized to h: {}, w: {}", height, width));
}

void GameWindow::handleResponse(std::string const& response) noexcept
{
	this->responseTab->appendContent(response);
	this->commandTab->refresh();
}

void GameWindow::handleEvent(std::string const& event) noexcept
{
	this->eventTab->appendContent(event);
	this->commandTab->refresh();
}
