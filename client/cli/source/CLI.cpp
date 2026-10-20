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


CLI::CLI(void) :
	UI(),
	commandPipe{ioUtils::createPipe()}
{
	this->pollFds[CLI::CLIENT].fd = this->gameClientSockets.first;
	this->pollFds[CLI::CLIENT].events = POLLIN;
	this->pollFds[CLI::CMD].fd = this->commandPipe.out;	// to send a full command to client
	this->pollFds[CLI::CMD].events = POLLIN;
	this->pollFds[CLI::RESIZE].fd = ioUtils::createSignalRedirectFd(SIGWINCH);	// redirected signal resize to this fd
	this->pollFds[CLI::RESIZE].events = POLLIN;
	this->pollFds[CLI::STDIN].fd = STDIN_FILENO;		// to handle any user input
	this->pollFds[CLI::STDIN].events = 0;

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
		LOG_WARN(LogContext::CLI, std::format("Window too small, triggering resize to h: {}, w: {}", this->height, this->width));
	}

	::initscr();		// NB check if those functions fail
	::cbreak();
	::noecho();
	curs_set(1);
	if (::has_colors() == false)
		LOG_WARN(LogContext::CLI, "Colors not supported");
	else
	{
		::start_color();
		::init_pair(NC_BLUE_COLOR, COLOR_BLUE, COLOR_BLACK);
		::init_pair(NC_RED_COLOR, COLOR_RED, COLOR_BLACK);
		::init_pair(NC_GREEN_COLOR, COLOR_GREEN, COLOR_BLACK);
		::init_pair(NC_YELLOW_COLOR, COLOR_YELLOW, COLOR_BLACK);
		::init_pair(NC_CYAN_COLOR, COLOR_CYAN, COLOR_BLACK);
	}

	this->loginWin = std::make_unique<LoginWindow>(this->commandPipe.in, this);
	this->newPlayerWin = std::make_unique<PlayerCreateWindow>(this->commandPipe.in, this);
	this->gameWin = std::make_unique<GameWindow>(this->commandPipe.in, this);
	this->errorWin = std::make_unique<ErrorWindow>(this);

	LOG_INFO(LogContext::CLI, "Done setup CLI");
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
	LOG_DEBUG(LogContext::CLI, "Cleaned Ncurses data");

	ioUtils::close(this->pollFds[CLI::RESIZE].fd);
	ioUtils::closePipe(this->commandPipe);
}

void CLI::start(void)
{
	UI::start();

	this->keepAlive = true;
	while (this->keepAlive == true)
	{
		try
		{
			this->pollFds[CLI::CLIENT].revents = 0;
			this->pollFds[CLI::STDIN].revents = 0;
			this->pollFds[CLI::RESIZE].revents = 0;
			this->pollFds[CLI::CMD].revents = 0;
			ioUtils::poll(this->pollFds.data(), this->pollFds.size(), -1);

			// read and show data from server 
			if (this->pollFds[CLI::CLIENT].revents & POLLIN)
				this->readFromServer();
			// send data to server
			if (this->pollFds[CLI::CLIENT].revents & POLLOUT)
				this->writeToServer();
			// handle error
			if (this->pollFds[CLI::CLIENT].revents & (POLLHUP | POLLERR | POLLNVAL))
				this->handlePollError();
			// read command from UI (and forward it to server)
			if (this->pollFds[CLI::CMD].revents & POLLIN)
				this->handleCommand("");
			// resize window
			if (this->pollFds[CLI::RESIZE].revents & POLLIN)
				this->handleResize();
			// user type input
			if (this->pollFds[CLI::STDIN].revents & POLLIN)
				this->currentWindow->handleUserInput();
		}
		catch (AppException const& e)
		{
			ErrorData err = e.getError();
			this->handleException(err);
		}
	}
	LOG_INFO(LogContext::CLI, "CLI stopped");
}

void CLI::stop(void) noexcept
{
	UI::stop();
	this->keepAlive = false;
}

void CLI::switchWindow(std::optional<GamePhase> newPhase)
{
	UI::updatePhase(newPhase);

	if (this->currentWindow and (this->phase != GamePhase::ERROR))
		this->currentWindow->clear();

	switch (this->phase)
	{
		case GamePhase::LOGIN:			this->currentWindow = this->loginWin.get(); break;
		case GamePhase::PLAYER_CREATE:	this->currentWindow = this->newPlayerWin.get(); break;
		case GamePhase::GAME:			this->currentWindow = this->gameWin.get(); break;
		case GamePhase::ERROR:			this->currentWindow = this->errorWin.get(); break;
		default:						assert(false and "phase not mapped to any window");
	}
	this->currentWindow->draw(this->height, this->width);
}

void CLI::handleResize(void)
{
	struct signalfd_siginfo si;
	ioUtils::read(this->pollFds[CLI::RESIZE].fd, &si, sizeof(si));		// I just needed to trigger the resize, flush any data

	struct winsize termSize;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &termSize) == -1)
	{
		LOG_WARN(LogContext::CLI, "Couldn't fetch terminal size, resize failed");
		return;
	}

	this->height = termSize.ws_row;
	this->width = termSize.ws_col;

	::resizeterm(this->height, this->width);
	if (this->currentWindow)
		this->currentWindow->resize(this->height, this->width);
}

void CLI::handlePollError(void)
{
	if (this->pollFds[CLIENT].revents & POLLHUP)
		throw AppException(ErrorCode::CLIENT_DISCONNECTED);
	else if (this->pollFds[CLIENT].revents & POLLNVAL)
		throw AppException(ErrorCode::IO_POLL_FAILED, "invalid socket (POLLNVAL)");
	else if (this->pollFds[CLIENT].revents & POLLERR)	// something actually went wrong with poll
	{
		int32_t sockErr = 0;
		socklen_t len = sizeof(sockErr);
	
		if (ioUtils::getsockopt(this->gameClientSockets.first, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, std::format("getsockopt failed during POLLERR: {}", ::strerror(errno)));
		else if (sockErr != 0)
			throw AppException(ErrorCode::IO_POLL_FAILED, ::strerror(sockErr));
	}
}

void CLI::shakeHands(void) noexcept
{
	UI::shakeHands();

	::flushinp();		// flush what might have been typed between window creation and handshake
	this->pollFds[CLI::STDIN].events = POLLIN;
}

void CLI::handleException(ErrorData const& error)
{
	if (error.code == ErrorCode::UI_INVALID_SIZE)		// trigger a resize
	{
		// currentWindow::draw() or currentWindow::resize() failed, clean the tabs half-built
		this->currentWindow->clear();

		int32_t height = this->height, width = this->width;
		if (height < Config::MIN_HEIGHT_CLI) height = Config::MIN_HEIGHT_CLI;
		if (width < Config::MIN_WIDTH_CLI) width = Config::MIN_WIDTH_CLI;

		std::cout << std::format("\033[8;{};{}t", height, width) << std::endl;
		LOG_WARN(LogContext::CLI, std::format("Window too small, triggering resize to h: {}, w: {}", height, width));
		return;
	}

	if (this->phase == GamePhase::ERROR)
		this->errorWin->adaptWinToError({ErrorCode::UI_DOUBLE_ERROR, mapError(error)});		// NB check this
	else
		this->errorWin->adaptWinToError(error);

	UI::handleException(error);
}

void CLI::handleCommand(std::string const& input)
{
	(void) input;
	char buffer[Config::CMD_BUFFER_SIZE];
	ssize_t n = 0L;

	try
	{
		n = ioUtils::read(this->pollFds[CLI::CMD].fd, buffer, Config::CMD_BUFFER_SIZE);
	}
	catch (AppException& e)
	{
		if (e.getError().code == ErrorCode::IO_READ_FAILED)
			e.getError().info += ", couldn't read command";
		throw e;
	}

	UI::handleCommand(std::string(buffer, n));
}

void CLI::showResponse(Message const& response) noexcept
{
	GameWindow* gameWin = dynamic_cast<GameWindow*>(this->gameWin.get());
	assert(gameWin != nullptr and "current window doesn't support handling a response");
	assert(!this->commandQueue.empty() and "no command in queue");

	gameWin->appendResponse(response, this->commandQueue.front().getCommandType());
	if (this->phase == GamePhase::GAME)
		gameWin->updateContentWindow();

	LOG_DEBUG(LogContext::CLI, std::format("Got response: '{}'", response.getRawMessage()));
}

void CLI::showEvent(Message const& event) noexcept
{
	GameWindow* gameWin = dynamic_cast<GameWindow*>(this->gameWin.get());
	assert(gameWin != nullptr and "current window doesn't support handling an event");

	gameWin->appendEvent(event);
	if (this->phase == GamePhase::GAME)
		gameWin->updateContentWindow();

	LOG_DEBUG(LogContext::CLI, std::format("Got event: '{}'", event.getRawMessage()));
}

void CLI::showError(Message const& error) noexcept
{
	GameWindow* gameWin = dynamic_cast<GameWindow*>(this->gameWin.get());
	assert(gameWin != nullptr and "current window doesn't support handling an event");
	assert(!this->commandQueue.empty() and "no command in queue");

	gameWin->appendError(error, this->commandQueue.front().getCommandType());
	if (this->phase == GamePhase::GAME)
		gameWin->updateContentWindow();

	LOG_DEBUG(LogContext::CLI, std::format("Got error: '{}'", error.getRawMessage()));
}


std::unique_ptr<UI> uiFactory()
{
	return std::make_unique<CLI>();
}
