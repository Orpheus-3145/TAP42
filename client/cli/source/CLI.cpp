#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>


CLI::CLI(int32_t clientSocket) :
	UI(clientSocket)
{
	::memset(this->pollFds, 0, CLI::POLL_SIZE * sizeof(struct pollfd));
	this->pollFds[CLI::STDIN_INDEX].fd = STDIN_FILENO;
	this->pollFds[CLI::RES_INDEX].fd = ioUtils::createSignalRedirectFd(SIGWINCH);
	this->pollFds[CLI::SOCK_INDEX].fd = clientSocket;

	this->dispatcher[KEY_LEFT]      = [this] { this->commandTab->moveCursorLeft(); };
	this->dispatcher[KEY_RIGHT]     = [this] { this->commandTab->moveCursorRight(); };
	// this->dispatcher['\t']          = [this] { this->switchForwardTab(); };
	// this->dispatcher[KEY_BTAB]      = [this] { this->switchBackwardTab(); };
	// KEY_HOME / KEY_END: not mapped
	this->dispatcher[KEY_DC]        = [this] { this->commandTab->deleteCharForward(); };
	this->dispatcher[127]           = [this] { this->commandTab->deleteCharBack(); };
	this->dispatcher[KEY_BACKSPACE] = [this] { this->commandTab->deleteCharBack(); };
	this->dispatcher[KEY_UP]        = [this] { this->commandTab->suggestPrevious(); };
	this->dispatcher[KEY_DOWN]      = [this] { this->commandTab->suggestNext(); };

	::initscr();
	::cbreak();
	::noecho();

	this->createWindow();
	this->refresh();

	LOG_INFO(LogContext::INTERFACE, "Done setup CLI");
}

CLI::~CLI(void) noexcept
{
	// empty memory manually because endwin has to be last
	// ncurses function to be called
	this->frame.reset();
	this->commandTab.reset();
	this->responseTab.reset();
	this->eventTab.reset();

	::endwin();
	ioUtils::close(this->pollFds[CLI::RES_INDEX]. fd);

	LOG_INFO(LogContext::INTERFACE, "CLI stopped");
}

void CLI::loop(void)
{
	while (this->keepAlive == true)
	{
		this->pollFds[CLI::STDIN_INDEX].events |= POLLIN;
		this->pollFds[CLI::STDIN_INDEX].revents = 0;
		this->pollFds[CLI::RES_INDEX].events |= POLLIN;
		this->pollFds[CLI::RES_INDEX].revents = 0;
		this->pollFds[CLI::SOCK_INDEX].events |= POLLIN;
		this->pollFds[CLI::SOCK_INDEX].revents = 0;
		ioUtils::poll(this->pollFds, POLL_SIZE, -1);

		if (pollFds[STDIN_INDEX].revents & POLLIN)
			this->handleUserInput();

		if (pollFds[RES_INDEX].revents & POLLIN)
			this->handleResizeEvent();

		if (pollFds[SOCK_INDEX].revents & POLLIN)
			this->readDataFromServer(this->pollFds[CLI::SOCK_INDEX].fd);

		if (pollFds[SOCK_INDEX].revents & POLLOUT)
			this->handleCommand();

		// client closed connection (because server did so) (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (pollFds[SOCK_INDEX].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_WARN(LogContext::INTERFACE, "Client HTTP unexpectedly terminated connection, closing session");
			this->exitLoop();
		}
		this->refresh();
	}
	LOG_INFO(LogContext::INTERFACE, "Ended CLI loop");
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
	this->responseTab = std::make_unique<OutputTab>(height, width, starty, startx, 0);
	this->responseTab->appendContent("This is where responses are shown");

	starty += height;
	this->eventTab = std::make_unique<OutputTab>(height, width, starty, startx, 0);
	this->eventTab->appendContent("This is where events are shown");
	this->commandTab->refresh();
}

void CLI::handleCommand(void)
{
	std::string command = this->commandTab->getLastInput();
	if (command == "")
		return;

	// if necessary parse/format command
	if (this->forwardDataToServer(this->pollFds[CLI::SOCK_INDEX].fd, command) == true)
		this->pollFds[CLI::SOCK_INDEX].events = 0;			// is everything has been sent stop poll for writing
}

void CLI::handleResponse(std::string const& response) noexcept
{
	this->responseTab->appendContent(response);
	this->commandTab->refresh();
}

void CLI::handleEvent(std::string const& event) noexcept
{
	this->eventTab->appendContent(event);
	this->commandTab->refresh();
}

void CLI::handleResizeEvent(void)
{
	struct signalfd_siginfo si;
	ioUtils::read(this->pollFds[CLI::RES_INDEX].fd, &si, sizeof(si));		// I don't care about the data, flush it

	struct winsize windowSize;
	::ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize);	// get the size of the resized terminal

	this->resize(windowSize.ws_row, windowSize.ws_col);
}

void CLI::resize(int32_t height, int32_t width)
{
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
}

void CLI::handleUserInput(void)
{
	int32_t inputChar = this->commandTab->getChar();	// this is blocking

	// special characters handling
	auto it = this->dispatcher.find(inputChar);
	if (it != this->dispatcher.end()) {
		it->second();
		return;
	}

	// Default handling
	this->commandTab->setChar(inputChar);
	if (inputChar != COMMAND_TERM)
		return;

	if (this->commandTab->getLastInput().empty() == false)
		this->pollFds[CLI::SOCK_INDEX].events |= POLLOUT;
}
