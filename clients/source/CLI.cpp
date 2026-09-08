#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>


CLI::CLI(int32_t commandFd, int32_t height, int32_t width) :
	GameInterface(commandFd, height, width),
	resizeFd{ioUtils::createSignalRedirectFd(SIGWINCH)}
{
	this->_dispatcher[KEY_LEFT]      = [this] { this->commandTab->moveCursorLeft(); };
	this->_dispatcher[KEY_RIGHT]     = [this] { this->commandTab->moveCursorRight(); };
	// this->_dispatcher['\t']          = [this] { this->switchForwardTab(); };
	// this->_dispatcher[KEY_BTAB]      = [this] { this->switchBackwardTab(); };
	// KEY_HOME / KEY_END: not mapped
	this->_dispatcher[KEY_DC]        = [this] { this->commandTab->deleteCharForward(); };
	this->_dispatcher[127]           = [this] { this->commandTab->deleteCharBack(); };
	this->_dispatcher[KEY_BACKSPACE] = [this] { this->commandTab->deleteCharBack(); };
	this->_dispatcher[KEY_UP]        = [this] { this->commandTab->suggestNextHint(); };
	this->_dispatcher[KEY_DOWN]      = [this] { this->commandTab->suggestPastHint(); };

	this->createWindow(height, width);

	LOG_INFO(LogContext::INTERFACE, "Setup for CLI done");
}

CLI::~CLI(void) noexcept		// NB check if it calls the parent destr.
{
	::endwin();
	ioUtils::close(this->resizeFd);

	LOG_INFO(LogContext::INTERFACE, "CLI stopped");
}

void CLI::loop(void)
{
	size_t nFds = 2;
	std::vector<struct pollfd> pollFds(nFds);
	// user input
	pollFds[0].fd = STDIN_FILENO;
	// resize signal redirect
	pollFds[1].fd = this->resizeFd;

	this->refresh();
	while (this->KeepAlive == true)
	{
		pollFds[0].events = POLLIN;
		pollFds[0].revents = 0;
		pollFds[1].events = POLLIN;
		pollFds[1].revents = 0;

		if (ioUtils::poll(pollFds.data(), nFds, -1) == -1)
		{
			if (errno == EINTR)
				continue;
			LOG_ERROR(LogContext::INTERFACE, "Poll failed: " + std::string(strerror(errno)));
			throw InterfaceException("poll failed: " + std::string(strerror(errno)));
		}

		if (pollFds[0].revents & POLLIN)
			this->dispatchUserInput();

		// a POLLIN means there's been a resize (signal SIGWENCH)
		if (pollFds[1].revents & POLLIN)
			this->resize(-1, -1);

		this->refresh();
	}
}

void CLI::handleResponse(std::string const& response)
{
	{
		std::lock_guard<std::mutex> lock(this->respMutex);
		this->responseTab->appendContent(response);
	}
	this->commandTab->refresh();
	this->refresh();		// manually refresh because main thread is polling
}

void CLI::handleEvent(std::string const& event)
{
	{
		std::lock_guard<std::mutex> lock(this->eventMutex);
		this->eventTab->appendContent(event);
	}
	this->commandTab->refresh();
	this->refresh();		// manually refresh because main thread is polling
}

void CLI::forwardCommandToServer(std::string const& command)
{
	GameInterface::forwardCommandToServer(command);

	if (command == QUIT)
		this->KeepAlive = false;
}

void CLI::createWindow(int32_t height, int32_t width)
{
	// adjust window size
	// printf("\033[8;%d;%dt", height, width);
	// fflush(stdout);

	::initscr();
	::cbreak();
	::noecho();

	int32_t winHeigth = (height % 2) == 0 ? height : height - 1;
	int32_t winWidth = (width % 2) == 0 ? width : width - 1;
	int32_t startx = 0;
	int32_t starty = 0;
	this->frame = std::make_unique<BasicTab>(winHeigth, winWidth, startx, starty, 0);

	winHeigth -= 2;
	winWidth = (winWidth - 2) / 2;
	startx += 1;
	starty += 1;
	this->commandTab = std::make_unique<InputTab>(winHeigth, winWidth, startx, starty, 0);

	winHeigth /= 2;
	startx += winWidth;
	this->responseTab = std::make_unique<OutputTab>(winHeigth, winWidth, starty, startx, 0);

	starty += winHeigth;
	this->eventTab = std::make_unique<OutputTab>(winHeigth, winWidth, starty, startx, 0);

	this->commandTab->appendContent("Insert some shit, type 'quit' to close");
	this->responseTab->appendContent("This is where responses are shown");
	this->eventTab->appendContent("This is where events are shown");
	this->commandTab->refresh();
}

void CLI::resize(int32_t height, int32_t width)
{
	(void) height;
	(void) width;

	struct signalfd_siginfo si;
	ioUtils::read(this->resizeFd, &si, sizeof(si));		// I don't care about the data, flush it

	struct winsize ws;
	::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);	// get the size of the resized terminal
	::resizeterm(ws.ws_row, ws.ws_col);

	int32_t winHeigth = (ws.ws_row % 2) == 0 ? ws.ws_row : ws.ws_row - 1;
	int32_t winWidth = (ws.ws_col % 2) == 0 ? ws.ws_col : ws.ws_col - 1;
	int32_t startx = 0;
	int32_t starty = 0;
	this->frame->resize(winHeigth, winWidth, startx, starty);

	winHeigth -= 2;
	winWidth = (winWidth - 2) / 2;
	startx += 1;
	starty += 1;
	this->commandTab->resize(winHeigth, winWidth, startx, starty);

	winHeigth /= 2;
	startx += winWidth;
	this->responseTab->resize(winHeigth, winWidth, starty, startx);

	starty += winHeigth;
	this->eventTab->resize(winHeigth, winWidth, starty, startx);

	this->commandTab->refresh();
	// because resize is not handled by ncurses there might be some garbage to read, flush it
	// this->commandTab->getChar();

}

void CLI::dispatchUserInput(void)
{
	int32_t inputChar = this->commandTab->getChar();	// this is blocking

	auto it = this->_dispatcher.find(inputChar);
	if (it != this->_dispatcher.end()) {
		it->second();
		return;
	}

	// Default handling
	this->commandTab->setChar(inputChar);
	if (inputChar == COMMAND_TERM)
	{
		std::string command = this->commandTab->getLastInput();
		this->forwardCommandToServer(command);
	}
}

// InputTab& CLI::getCurrentTab(void)
// {
// 	assert(this->currentTabIndex < this->inputTabs.size() and "index overflow whiele accessing input tabs");
// 	return this->inputTabs.at(this->currentTabIndex);
// }

// void CLI::switchForwardTab(void) noexcept
// {
// 	if (this->currentTabIndex < CLI::N_TABS - 1)
// 		this->currentTabIndex++;
// 	else
// 		this->currentTabIndex = 0UL;
// }

// void CLI::switchBackwardTab(void) noexcept
// {
// 	if (this->currentTabIndex > 0UL)
// 		this->currentTabIndex--;
// 	else
// 		this->currentTabIndex = CLI::N_TABS - 1;
// }

// void CLI::setCurrentTab(size_t newTabIndex) noexcept
// {
// 	assert(newTabIndex < CLI::N_TABS);
// 	this->currentTabIndex = newTabIndex;
// }

