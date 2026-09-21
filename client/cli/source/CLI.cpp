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

	int32_t height, width;
	this->getTerminalSize(height, width);

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
	curs_set(1);
	if (::has_colors() == false)
		LOG_WARN(LogContext::INTERFACE, "Colors not supported");
	else
	{
		::start_color();
		::init_pair(BLUE_COLOR, COLOR_BLUE, COLOR_BLACK);
		::init_pair(RED_COLOR, COLOR_RED, COLOR_BLACK);
		::init_pair(GREEN_COLOR, COLOR_GREEN, COLOR_BLACK);
		::init_pair(YELLOW_COLOR, COLOR_YELLOW, COLOR_BLACK);
	}
	// // for callback (scrolling tabs) with mouse wheel
    // ::mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED | ALL_MOUSE_EVENTS, NULL);
    // ::mouseinterval(0);       // disable delayed click
	
	this->loginWin = std::make_unique<LoginWindow>(this->commandPipe.in);
	this->newPlayerWin = std::make_unique<PlayerCreateWindow>(this->commandPipe.in);
	this->gameWin = std::make_unique<GameWindow>(this->commandPipe.in, this->chatPipe.in);
	// this->errorWin = std::make_unique<CurseWindow>();

	LOG_INFO(LogContext::INTERFACE, "Done setup CLI");
	LOG_DEBUG(LogContext::INTERFACE, std::format("Listening to client socket: {}", this->pollFds[CLI::CLIENT].fd));
}

CLI::~CLI(void) noexcept
{
	// empty memory manually because endwin() has to be last
	// ncurses function to be called
	this->loginWin.reset();
	this->newPlayerWin.reset();
	this->gameWin.reset();
	// this->errorWin.reset();

	::endwin();
	LOG_DEBUG(LogContext::INTERFACE, std::format("Cleaned Ncurses data"));

	ioUtils::closePipe(this->commandPipe);
	ioUtils::closePipe(this->chatPipe);
	ioUtils::close(this->pollFds[CLI::RESIZE].fd);
}

void CLI::start(void)
{
	this->loginPhase();

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

		// user type input
		if (this->pollFds[STDIN].revents & POLLIN)
			this->currentWindow->getActiveTab()->handleUserInput();
		// resize window
		if (this->pollFds[RESIZE].revents & POLLIN)
			this->handleResize();
		// read and show data from server 
		if (this->pollFds[CLIENT].revents & POLLIN)
			this->readInputFromServer();
		// send data to server
		if (this->pollFds[CLIENT].revents & POLLOUT)
		{
			this->writeInputToServer();
			if (this->toServerSize == 0UL)
				this->pollFds[CLI::CLIENT].events = POLLIN;
		}
		// handle error
		if (this->pollFds[CLIENT].revents & (POLLHUP | POLLERR | POLLNVAL))
			this->handlePollError();
		// read command from UI (and forward it to server)
		if (this->pollFds[CMD].revents & POLLIN)
			this->handleGameCommand();
		// read chat msg/command from UI (and forward it to server)
		if (this->pollFds[CHAT].revents & POLLIN)
			this->handleChatCommand();

		this->currentWindow->refresh();
	}
	LOG_INFO(LogContext::INTERFACE, "CLI stopped");
}

void CLI::getTerminalSize(int32_t& height, int32_t& width) const noexcept
{
	struct winsize termSize;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &termSize) == -1)
	{
		LOG_WARN(LogContext::INTERFACE, "Failed to fetch terminal size, using standard dimension");
		height = Config::MIN_HEIGHT_CLI;
		width = Config::MIN_WIDTH_CLI;
	}
	else
	{
		height = termSize.ws_row;
		width = termSize.ws_col;
	}
}

void CLI::handleResize(void)
{
	struct signalfd_siginfo si;
	ioUtils::read(this->pollFds[CLI::RESIZE].fd, &si, sizeof(si));		// I don't care about the data, flush it

	int32_t height, width;
	this->getTerminalSize(height, width);

	this->currentWindow->resize(height, width);
}

void CLI::handlePollError(void) noexcept
{
	if (this->pollFds[CLIENT].revents & POLLHUP)		// HTTP client stopped
		this->handleError("...");
	else if (this->pollFds[CLIENT].revents & POLLERR)	// socket is invalid (poll didn't fail)
		this->handleError("...");
	else if (this->pollFds[CLIENT].revents & POLLNVAL)	// something actually went wrong with poll
	{
		int32_t sockErr = 0;
		socklen_t len = sizeof(sockErr);
	
		if (ioUtils::getsockopt(this->clientSocket, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0)	// getsockopt could also fail (check strerror(errno))
			this->handleError("...");
		else if (sockErr != 0)		// log, show error tab and close win (check strerror(sockErr))
			this->handleError("...");
	}
}

void CLI::handleGameCommand(void)
{
	char buffer[Config::BUFF_SIZE];

	try
	{
		ssize_t n = ioUtils::read(this->commandPipe.out, buffer, Config::BUFF_SIZE);

		if ((this->phase == GamePhase::LOGIN) or (this->phase == GamePhase::PLAYER_CREATE))
		{
			// move to the right to insert CMD_CONNECT and a space at the beginning of the command
			::memmove(buffer + ::strlen(CMD_CONNECT) + 1, buffer, n);
			::memcpy(buffer + ::strlen(CMD_CONNECT), &COMMAND_SP, 1);
			::memcpy(buffer, CMD_CONNECT, ::strlen(CMD_CONNECT));
			n += ::strlen(CMD_CONNECT) + 1;
		}

		::memcpy(buffer + n, &COMMAND_TERM, 1);
		n++;
		// store formatted command, ready to be sento to client
		::memcpy(this->toServerBuffer + this->toServerSize, buffer, n);
		this->toServerSize += n;
		this->pollFds[CLI::CLIENT].events |= POLLOUT;
	}
	catch(const IOException& e)
	{
		this->handleError(std::format("I/O error failed to write to client: '{}'", e.what()));
	}
}

void CLI::handleChatCommand(void)
{
	char buffer[Config::BUFF_SIZE];

	try
	{
		ssize_t n = ioUtils::read(this->chatPipe.out, buffer, Config::BUFF_SIZE);

		if (this->phase == GamePhase::GAME)
		{
			GameWindow* gameWin = dynamic_cast<GameWindow*>(this->currentWindow);
			assert(gameWin != nullptr and "current window doesn't support handling a response");
			gameWin->showChatMsg(Config::PROMPT + std::string(buffer, n));
		}

		::memcpy(buffer + n, &COMMAND_TERM, 1);
		n++;
		// store formatted command, ready to be sento to client
		::memcpy(this->toServerBuffer + this->toServerSize, buffer, n);
		this->toServerSize += n;
		this->pollFds[CLI::CLIENT].events |= POLLOUT;
	}
	catch(const IOException& e)
	{
		this->handleError(std::format("I/O error failed to write to client: '{}'", e.what()));
	}
}

void CLI::loginPhase(void)
{
	UI::loginPhase();

	int32_t height, width;
	this->getTerminalSize(height, width);

	this->currentWindow = this->loginWin.get();
	this->currentWindow->draw(height, width);
}

void CLI::newPlayerPhase(void)
{
	UI::newPlayerPhase();

	int32_t height, width;
	this->getTerminalSize(height, width);

	if (this->currentWindow)
		this->currentWindow->clear();
	this->currentWindow = this->newPlayerWin.get();
	this->currentWindow->draw(height, width);
}

void CLI::gamePhase(void)
{
	UI::gamePhase();

	int32_t height, width;
	this->getTerminalSize(height, width);

	if (this->currentWindow)
		this->currentWindow->clear();
	this->currentWindow = this->gameWin.get();
	this->currentWindow->draw(height, width);
}

void CLI::handleError(std::string const& errMsg) noexcept
{
	UI::handleError(errMsg);

	int32_t height, width;
	this->getTerminalSize(height, width);

	if (this->currentWindow)
		this->currentWindow->clear();
	// this->error.set(errMsg);		or smt
	// this->currentWindow = this->errorWin.get();
	this->currentWindow->draw(height, width);
}

void CLI::showResponse(std::string const& response) noexcept
{
	GameWindow* gameWin = dynamic_cast<GameWindow*>(this->currentWindow);
	assert(gameWin != nullptr and "current window doesn't support handling a response");

	// if (response is chat type)
	// 	gameWin->showChatMsg(response);
	// else
	gameWin->showResponse(response);
}

void CLI::showEvent(std::string const& event) noexcept
{
	GameWindow* gameWin = dynamic_cast<GameWindow*>(this->currentWindow);
	assert(gameWin != nullptr and "current window doesn't support handling an event");

	// if (event is chat type)
	// 	gameWin->showChatMsg(event);
	// else
	gameWin->showEvent(event);
}

std::unique_ptr<UI> uiFactory(int32_t clientSocket)
{
	return std::make_unique<CLI>(clientSocket);
}
