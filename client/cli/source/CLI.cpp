#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>


CLI::CLI(int32_t commandFd) :
	UI(commandFd),
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
	this->_dispatcher[KEY_UP]        = [this] { this->commandTab->suggestPrevious(); };
	this->_dispatcher[KEY_DOWN]      = [this] { this->commandTab->suggestNext(); };

	::initscr();
	::cbreak();
	::noecho();

	this->createWindow();

	LOG_INFO(LogContext::INTERFACE, "Done setup CLI");
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

		// user key input
		if (pollFds[0].revents & POLLIN)
			this->dispatchUserInput();

		// means there's been a resize (signal SIGWENCH)
		// redisrected to a fd, hence the POLLIN
		if (pollFds[1].revents & POLLIN)
			this->handleResizeEvent();

		this->refresh();
	}
	LOG_INFO(LogContext::GAME_CLIENT, "Ended UI loop");

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

void CLI::createWindow(void)
{
	int32_t height = (LINES % 2) == 0 ? LINES : LINES - 1;
	int32_t width = (COLS % 2) != 0 ? COLS : COLS - 1;
	int32_t starty = 0;
	int32_t startx = 0;
	this->frame = std::make_unique<OutputTab>(height, width, starty, startx, 0);

	height -= 2;
	width = (width - 5) / 2;
	startx += 2;
	starty += 1;
	this->commandTab = std::make_unique<InputTab>(height, width, starty, startx, 0);
	this->commandTab->appendContent("This is where the input is shown");

	height /= 2;
	startx += width + 1;
	{
		std::lock_guard<std::mutex> lock(this->respMutex);
		this->responseTab = std::make_unique<OutputTab>(height, width, starty, startx, 0);
		this->responseTab->appendContent("This is where responses are shown");
	}

	starty += height;
	{
		std::lock_guard<std::mutex> lock(this->eventMutex);
		this->eventTab = std::make_unique<OutputTab>(height, width, starty, startx, 0);
		this->eventTab->appendContent("This is where events are shown");
	}
	this->commandTab->refresh();
}

void CLI::handleResizeEvent(void)
{
	struct signalfd_siginfo si;
	ioUtils::read(this->resizeFd, &si, sizeof(si));		// I don't care about the data, flush it

	struct winsize windowSize;
	::ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize);	// get the size of the resized terminal

	this->resize(windowSize.ws_row, windowSize.ws_col);
}

void CLI::resize(int32_t height, int32_t width)
{
	LOG_DEBUG(LogContext::INTERFACE, "called resize");

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
	{
		std::lock_guard<std::mutex> lock(this->respMutex);
		this->responseTab->resize(height, width, starty, startx);
	}

	starty += height;
	{
		std::lock_guard<std::mutex> lock(this->eventMutex);
		this->eventTab->resize(height, width, starty, startx);
	}
	this->commandTab->refresh();

	// because resize is not handled by ncurses there might be some garbage to read, flush it
	::flushinp();
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
	if (inputChar != COMMAND_TERM)
		return;

	std::string command = this->commandTab->getLastInput();
	if (command.empty() == true)
		return;

	this->forwardCommandToServer(command);
	if (command == QUIT)
		this->KeepAlive = false;
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

