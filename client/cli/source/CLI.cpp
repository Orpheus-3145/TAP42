#include "CLI.hpp"
#include "BasicTab.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"
#include "Utils.hpp"

#include <format>
#include <cassert>
#include <cstring>
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
		throw AppException(ErrorCode::UI_FAILED_GET_TERM_SIZE);

	this->height = termSize.ws_row;
	this->width = termSize.ws_col;

	if ((this->height < Config::MIN_HEIGHT_CLI) or (this->width < Config::MIN_WIDTH_CLI))
	{
		if (this->height < Config::MIN_HEIGHT_CLI)
			this->height = Config::MIN_HEIGHT_CLI;
		if (this->width < Config::MIN_WIDTH_CLI)
			this->width = Config::MIN_WIDTH_CLI;

		std::cout << std::format("\033[8;{};{}t", this->height, this->width) << std::endl;
		LOG_WARN(LogContext::INTERFACE, std::format("Window too small, triggering resize to h: {}, w: {}", this->height, this->width));
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
		::init_pair(CYAN_COLOR, COLOR_CYAN, COLOR_BLACK);
	}
	// // for callback (scrolling tabs) with mouse wheel
    // ::mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED | ALL_MOUSE_EVENTS, NULL);
    // ::mouseinterval(0);       // disable delayed click
	
	this->loginWin = std::make_unique<LoginWindow>(this->commandPipe.in);
	this->newPlayerWin = std::make_unique<PlayerCreateWindow>(this->commandPipe.in);
	this->gameWin = std::make_unique<GameWindow>(this->commandPipe.in, this->chatPipe.in);
	this->errorWin = std::make_unique<ErrorWindow>();

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
	this->errorWin.reset();

	::endwin();
	LOG_DEBUG(LogContext::INTERFACE, std::format("Cleaned Ncurses data"));

	ioUtils::closePipe(this->commandPipe);
	ioUtils::closePipe(this->chatPipe);
	ioUtils::close(this->pollFds[CLI::RESIZE].fd);
}

void CLI::start(void)
{
	this->loginPhase();

	this->pollFds[CLI::STDIN].events = POLLIN;
	this->pollFds[CLI::RESIZE].events = POLLIN;
	this->pollFds[CLI::CLIENT].events = POLLIN;
	this->pollFds[CLI::CMD].events = POLLIN;
	this->pollFds[CLI::CHAT].events = POLLIN;

	while (this->keepAlive == true)
	{
		try
		{
			this->pollFds[CLI::STDIN].revents = 0;
			this->pollFds[CLI::RESIZE].revents = 0;
			this->pollFds[CLI::CLIENT].revents = 0;
			this->pollFds[CLI::CMD].revents = 0;
			this->pollFds[CLI::CHAT].revents = 0;
			ioUtils::poll(this->pollFds, POLL_SIZE, -1);
	
			// user type input
			if (this->pollFds[STDIN].revents & POLLIN)
				this->currentWindow->handleUserInput();
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
				if (this->toServerSize == 0UL)		// NB move this to UI
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
		}
		catch (AppException const& e)
		{
			this->handleError(e.getCode(), e.what());
		}

		this->currentWindow->refresh();
	}
	LOG_INFO(LogContext::INTERFACE, "CLI stopped");
}

void CLI::handleResize(void)
{
	struct signalfd_siginfo si;
	ioUtils::read(this->pollFds[CLI::RESIZE].fd, &si, sizeof(si));		// just need to trigger the resize, flush any data

	struct winsize termSize;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &termSize) == -1)
	{
		LOG_WARN(LogContext::INTERFACE, "Couldn't fetch terminal data, resize window failed");
		return;
	}

	this->currentWindow->resize(termSize.ws_row, termSize.ws_col);
}

void CLI::handlePollError(void)		// NB move to UI
{
	if (this->pollFds[CLIENT].revents & POLLHUP)
		throw AppException(ErrorCode::CLIENT_DISCONNECTED);
	else if (this->pollFds[CLIENT].revents & POLLNVAL)
		throw AppException(ErrorCode::IO_POLL_FAILED, "invalid scket (POLLNVAL)");
	else if (this->pollFds[CLIENT].revents & POLLERR)	// something actually went wrong with poll
	{
		int32_t sockErr = 0;
		socklen_t len = sizeof(sockErr);
	
		if (ioUtils::getsockopt(this->clientSocket, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, std::format("getsockopt failed during POLLERR: {}", ::strerror(errno)));
		else if (sockErr != 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, ::strerror(sockErr));
	}
}

void CLI::handleGameCommand(void)
{
	char buffer[Config::BUFF_SIZE];

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

void CLI::handleChatCommand(void)
{
	char buffer[Config::BUFF_SIZE];

	ssize_t n = ioUtils::read(this->chatPipe.out, buffer, Config::BUFF_SIZE);

	::memcpy(buffer + n, &COMMAND_TERM, 1);
	n++;
	// store formatted command, ready to be sento to client
	::memcpy(this->toServerBuffer + this->toServerSize, buffer, n);
	this->toServerSize += n;
	this->pollFds[CLI::CLIENT].events |= POLLOUT;
}

void CLI::loginPhase(void)
{
	UI::loginPhase();

	if (this->currentWindow)
		this->currentWindow->clear();

	this->currentWindow = this->loginWin.get();
	this->currentWindow->draw(this->height, this->width);
}

void CLI::newPlayerPhase(void)
{
	UI::newPlayerPhase();

	if (this->currentWindow)
		this->currentWindow->clear();

	this->currentWindow = this->newPlayerWin.get();
	this->currentWindow->draw(this->height, this->width);
}

void CLI::gamePhase(void)
{
	UI::gamePhase();

	if (this->currentWindow)
		this->currentWindow->clear();

	this->currentWindow = this->gameWin.get();
	this->currentWindow->draw(this->height, this->width);
}

void CLI::handleError(ErrorCode const& code, std::string const& errorInfo)
{
	if (code == ErrorCode::UI_INVALID_SIZE)
	{
		// currentWindow::draw() or currentWindow::resize() failed, clean the tabs half-built
		this->currentWindow->clear();

		int32_t height = this->height, width = this->width;
		if (height < Config::MIN_HEIGHT_CLI) height = Config::MIN_HEIGHT_CLI;
		if (width < Config::MIN_WIDTH_CLI) width = Config::MIN_WIDTH_CLI;

		std::cout << std::format("\033[8;{};{}t", height, width) << std::endl;
		LOG_WARN(LogContext::INTERFACE, std::format("Window too small, triggering resize to h: {}, w: {}", height, width));

		// NB manually resize here?
		// if this error happens when a window is created, this trigger
		// will call the resize on the window without having draw() called (it will show empty tabs)
		return;
	}

	GamePhase currentPhase = this->phase;
	if (currentPhase == GamePhase::ERROR)
	{
		LOG_ERROR(LogContext::INTERFACE, std::format("Got error: '{}' while handling a previous error", errorInfo));
		return;
	}
	UI::handleError(code, errorInfo);

	switch (code)
	{
		case ErrorCode::UI_USERNAME_NOT_EXISTS:
			this->errorWin->updateDescription(errorInfo);
			this->errorWin->setAction1("RETRY", [this] { this->loginPhase(); });
			this->errorWin->setAction2("CREATE NEW", [this] { this->newPlayerPhase(); });
			break;
		
		case ErrorCode::SERVER_ERROR:
			this->errorWin->updateDescription(errorInfo);
			this->errorWin->setAction1("BACK", [this, currentPhase] { 
				if (currentPhase == GamePhase::LOGIN)				this->loginPhase();
				else if (currentPhase == GamePhase::PLAYER_CREATE)	this->newPlayerPhase();
				else if (currentPhase == GamePhase::GAME)			this->gamePhase();
			});
			this->errorWin->setAction2("CLOSE", [this] { this->stop(); });
			break;

		default:
			this->errorWin->updateDescription(errorInfo);
			this->errorWin->setAction1("CLOSE", [this] { this->stop(); });
			break;
	}

	if (this->currentWindow)
		this->currentWindow->clear();
	this->currentWindow = this->errorWin.get();
	this->currentWindow->draw(this->height, this->width);
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
