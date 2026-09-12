#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <format>
#include <cassert>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>


CLI::CLI(int32_t clientSocket) :
	UI(clientSocket)
{
	::memset(this->pollFds, 0, CLI::POLL_SIZE * sizeof(struct pollfd));
	this->pollFds[CLI::I_STDIN].fd = STDIN_FILENO;
	this->pollFds[CLI::I_RESIZE].fd = ioUtils::createSignalRedirectFd(SIGWINCH);
	this->pollFds[CLI::I_CLIENT].fd = clientSocket;

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
	LOG_DEBUG(LogContext::INTERFACE, std::format("Listening to client socket: {}", this->pollFds[CLI::I_CLIENT].fd));
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
	ioUtils::close(this->pollFds[CLI::I_RESIZE]. fd);
}

void CLI::startUI(void)
{
	while (this->keepAlive == true)
	{
		this->pollFds[CLI::I_STDIN].events |= POLLIN;
		this->pollFds[CLI::I_STDIN].revents = 0;
		this->pollFds[CLI::I_RESIZE].events |= POLLIN;
		this->pollFds[CLI::I_RESIZE].revents = 0;
		this->pollFds[CLI::I_CLIENT].events |= POLLIN;
		this->pollFds[CLI::I_CLIENT].revents = 0;
		ioUtils::poll(this->pollFds, POLL_SIZE, -1);

		if (pollFds[I_STDIN].revents & POLLIN)
			this->handleUserInput();

		if (pollFds[I_RESIZE].revents & POLLIN)
			this->handleResizeEvent();

		if (pollFds[I_CLIENT].revents & POLLIN)
			this->readDataFromServer();

		if (pollFds[I_CLIENT].revents & POLLOUT)
			this->handleCommand(this->commandTab->getLastInput());

		// client closed connection (because server did so) (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (pollFds[I_CLIENT].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_WARN(LogContext::INTERFACE, "Client HTTP unexpectedly terminated connection, closing session");
			this->stopUI();
		}
		this->refresh();
	}
	LOG_INFO(LogContext::INTERFACE, "CLI stopped");
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

	LOG_DEBUG(LogContext::INTERFACE, std::format("CLI window size h: {}, w: {}", height, width));
}

void CLI::handleCommand(std::string const& command)
{
	if (command == "")
		return;

	// if necessary parse/format command
	if (this->writeDataToServer(command) == true)
		this->pollFds[CLI::I_CLIENT].events = 0;			// if everything has been sent end  poll writing
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
	ioUtils::read(this->pollFds[CLI::I_RESIZE].fd, &si, sizeof(si));		// I don't care about the data, flush it

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

	LOG_DEBUG(LogContext::INTERFACE, std::format("Window resized to h: {}, w: {}", height, width));
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
		this->pollFds[CLI::I_CLIENT].events |= POLLOUT;
}

std::unique_ptr<UI> uiFactory(int32_t clientSocket)
{
	return std::make_unique<CLI>(clientSocket);
}