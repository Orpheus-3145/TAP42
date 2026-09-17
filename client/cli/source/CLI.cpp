#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"
#include "Utils.hpp"

#include <format>
#include <cassert>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>


CLI::CLI(int32_t clientSocket) :
	UI(clientSocket)
{
	this->commandPipe = ioUtils::createPipe();
	this->chatPipe = ioUtils::createPipe();

	::memset(this->pollFds, 0, CLI::POLL_SIZE * sizeof(struct pollfd));
	this->pollFds[CLI::STDIN].fd = STDIN_FILENO;
	this->pollFds[CLI::RESIZE].fd = ioUtils::createSignalRedirectFd(SIGWINCH);
	this->pollFds[CLI::CLIENT].fd = clientSocket;
	this->pollFds[CLI::CMD].fd = this->commandPipe.out;
	this->pollFds[CLI::CHAT].fd = this->chatPipe.out;

	struct winsize termSize;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &termSize) == -1)
		throw CliException("Failed to fecth terminal size");

	int32_t height = termSize.ws_row;
	int32_t width = termSize.ws_col;

	if ((height < Config::MIN_HEIGHT_CLI) or (width < Config::MIN_WIDTH_CLI))
	{
		if (height < Config::MIN_HEIGHT_CLI)
			height = Config::MIN_HEIGHT_CLI;
		if (width < Config::MIN_WIDTH_CLI)
			width = Config::MIN_WIDTH_CLI;

		std::cout << std::format("\033[8;{};{}t", height, width) << std::endl;
		LOG_WARN(LogContext::INTERFACE, std::format("Window too small, forced to h: {}, w: {}", height, width));
	}

	::initscr();		// check if those functions fail
	::cbreak();
	::noecho();
	if (::has_colors() == false)
		LOG_WARN(LogContext::INTERFACE, "Colors not supported");
	else
		::start_color();

	this->game = std::make_unique<GameWindow>(height, width, this->commandPipe.in, this->chatPipe.in);
	// this->settings = std::make_unique<GameWindow>(height, width);
	// this->login = std::make_unique<GameWindow>(height, width);

	LOG_INFO(LogContext::INTERFACE, "Done setup CLI");
	LOG_DEBUG(LogContext::INTERFACE, std::format("Listening to client socket: {}", this->pollFds[CLI::CLIENT].fd));
}

CLI::~CLI(void) noexcept
{
	// empty memory manually because endwin() has to be last
	// ncurses function to be called
	this->game.reset();
	// this->settings.reset();
	// this->login.reset();

	::endwin();
	LOG_DEBUG(LogContext::INTERFACE, std::format("Cleaned Ncurses data"));

	ioUtils::closePipe(this->commandPipe);
	ioUtils::closePipe(this->chatPipe);
	ioUtils::close(this->pollFds[CLI::RESIZE].fd);
}

void CLI::start(void)
{
	while (this->keepAlive == true)
	{
		this->pollFds[CLI::STDIN].events |= POLLIN;
		this->pollFds[CLI::STDIN].revents = 0;
		this->pollFds[CLI::RESIZE].events |= POLLIN;
		this->pollFds[CLI::RESIZE].revents = 0;
		this->pollFds[CLI::CLIENT].events |= POLLIN;
		this->pollFds[CLI::CLIENT].revents = 0;
		this->pollFds[CLI::CMD].events |= POLLIN;
		this->pollFds[CLI::CMD].revents = 0;
		this->pollFds[CLI::CHAT].events |= POLLIN;
		this->pollFds[CLI::CHAT].revents = 0;
		ioUtils::poll(this->pollFds, POLL_SIZE, -1);

		if (this->pollFds[STDIN].revents & POLLIN)
			this->game->readInput();

		if (this->pollFds[RESIZE].revents & POLLIN)
			this->handleResize();

		if (this->pollFds[CLIENT].revents & POLLIN)
			this->handleInputFromServer();

		if (this->pollFds[CLIENT].revents & POLLOUT)
		{
			this->handleInputToServer();
			if (this->toServerSize == 0UL)
				this->pollFds[CLI::CLIENT].events = POLLIN;
		}

		if (this->pollFds[CLIENT].revents & (POLLHUP | POLLERR | POLLNVAL))
			this->handlePollError();

		if (this->pollFds[CMD].revents & POLLIN)
			this->handleGameCommand();

		if (this->pollFds[CHAT].revents & POLLIN)
			this->handleChatCommand();

		this->game->refresh();
	}
	LOG_INFO(LogContext::INTERFACE, "CLI stopped");
}

void CLI::handleResize(void)
{
	struct signalfd_siginfo si;
	ioUtils::read(this->pollFds[CLI::RESIZE].fd, &si, sizeof(si));		// I don't care about the data, flush it

	struct winsize windowSize;
	::ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize);	// get the size of the resized terminal

	// should do the current window
	this->game->resize(windowSize.ws_row, windowSize.ws_col);
}

void CLI::handlePollError(void) noexcept
{
	if (this->pollFds[CLIENT].revents & POLLHUP)
	{
		// HTTP client stopped, log, show error tab and close win
	}
	else if (this->pollFds[CLIENT].revents & POLLERR)
	{
		// socket is invalid (poll didn't fail), log, show error tab and close win
	}
	else if (this->pollFds[CLIENT].revents & POLLNVAL)
	{
		// something actually went wrong with poll
		int32_t sockErr = 0;
		socklen_t len = sizeof(sockErr);
	
		if (ioUtils::getsockopt(this->clientSocket, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0)
		{
			// getsockopt could also fail, log, show error tab and close win (check strerror(errno))
		}
		else if (sockErr != 0)
		{
			// log, show error tab and close win (check strerror(sockErr))
		}
	}
}

void CLI::handleGameCommand(void)
{
	int32_t commandPipe = this->pollFds[CMD].fd;
	// format command, convert it to HTTP command if necessary

	try
	{
		ssize_t n = ioUtils::read(commandPipe, this->toServerBuffer + this->toServerSize, Config::BUFF_SIZE - this->toServerSize);
		this->pollFds[CLI::CLIENT].events |= POLLOUT;

		this->toServerSize += n;
	}
	catch(const IOException& e)
	{
		std::string errMsg = std::format("I/O error failed to write to client: '{}'", e.what());
		LOG_ERROR(LogContext::INTERFACE, errMsg);
		this->handleError(errMsg);
	}
}

void CLI::handleChatCommand(void)
{
	int32_t chatPipe = this->pollFds[CHAT].fd;
	// format command, convert it to HTTP command if necessary

	try
	{
		ssize_t n = ioUtils::read(chatPipe, this->toServerBuffer + this->toServerSize, Config::BUFF_SIZE - this->toServerSize);
		this->pollFds[CLI::CLIENT].events |= POLLOUT;

		this->toServerSize += n;
	}
	catch(const IOException& e)
	{
		std::string errMsg = std::format("I/O error failed to write to client: '{}'", e.what());
		LOG_ERROR(LogContext::INTERFACE, errMsg);
		this->handleError(errMsg);
	}
}

void CLI::handleResponse(std::string const& response) noexcept
{
	this->game->handleResponse(response);
}

void CLI::handleEvent(std::string const& event) noexcept
{
	this->game->handleEvent(event);
}

std::unique_ptr<UI> uiFactory(int32_t clientSocket)
{
	return std::make_unique<CLI>(clientSocket);
}
