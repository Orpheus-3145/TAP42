#include "CLI.hpp"
#include "BasicTab.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <format>
#include <cassert>
#include <cstring>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>


CLI::CLI(void) : UI()
{
	this->pollFds.resize(CLI::POLL_SIZE);
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
		LOG_WARN(LogContext::INTERFACE, std::format("Window too small, triggering resize to h: {}, w: {}", this->height, this->width));
	}

	::initscr();		// NB check if those functions fail
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
	// for callback (scrolling tabs) with mouse wheel
    // ::mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED | ALL_MOUSE_EVENTS, NULL);
    // ::mouseinterval(0);       // disable delayed click

	this->loginWin = std::make_unique<LoginWindow>(this->commandPipe.in, this);
	this->newPlayerWin = std::make_unique<PlayerCreateWindow>(this->commandPipe.in, this);
	this->gameWin = std::make_unique<GameWindow>(this->commandPipe.in, this);
	this->errorWin = std::make_unique<ErrorWindow>(this);

	LOG_INFO(LogContext::INTERFACE, "Done setup CLI");
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

	ioUtils::close(this->pollFds[CLI::RESIZE].fd);
}

void CLI::start(void)
{
	UI::start();

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
				this->handleCommand();
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
			this->handleError(err);
		}
	}
	LOG_INFO(LogContext::INTERFACE, "CLI stopped");
}

void CLI::handleResize(void)
{
	struct signalfd_siginfo si;
	ioUtils::read(this->pollFds[CLI::RESIZE].fd, &si, sizeof(si));		// I just needed to trigger the resize, flush any data

	struct winsize termSize;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &termSize) == -1)
	{
		LOG_WARN(LogContext::INTERFACE, "Couldn't fetch terminal size, resize failed");
		return;
	}

	this->height = termSize.ws_row;
	this->width = termSize.ws_col;

	::resizeterm(this->height, this->width);
	if (this->currentWindow)
		this->currentWindow->resize(this->height, this->width);
}

void CLI::shakeHands(void) noexcept
{
	UI::shakeHands();

	::flushinp();		// flush what might have been typed between window creation and handshake
	this->pollFds[CLI::STDIN].events = POLLIN;
}

void CLI::handleError(ErrorData& error)
{
	if (error.code == ErrorCode::UI_INVALID_SIZE)		// trigger a resize
	{
		// currentWindow::draw() or currentWindow::resize() failed, clean the tabs half-built
		this->currentWindow->clear();

		int32_t height = this->height, width = this->width;
		if (height < Config::MIN_HEIGHT_CLI) height = Config::MIN_HEIGHT_CLI;
		if (width < Config::MIN_WIDTH_CLI) width = Config::MIN_WIDTH_CLI;

		std::cout << std::format("\033[8;{};{}t", height, width) << std::endl;
		LOG_WARN(LogContext::INTERFACE, std::format("Window too small, triggering resize to h: {}, w: {}", height, width));
		return;
	}
	UI::handleError(error);

	if (this->phase == GamePhase::ERROR)
	{
		// if there's another error happening already just print message and
		// stop session
		error.info = mapError(error);
		error.code = ErrorCode::UI_DOUBLE_ERROR;
	}

	this->errorWin->adaptWinToError(error);
	this->errorWin->setPreviousPhase(this->phase);
	this->switchWindow(GamePhase::ERROR);
}

void CLI::handleResponse(std::string const& response) noexcept
{
	GameWindow* gameWin = dynamic_cast<GameWindow*>(this->gameWin.get());
	assert(gameWin != nullptr and "current window doesn't support handling a response");

	// if (response is chat type)
	// 	gameWin->showChatMsg(response);
	// else
	gameWin->appendResponse(response);

	if (this->phase == GamePhase::GAME)
		gameWin->updateContentWindow();
}

void CLI::handleEvent(std::string const& event) noexcept
{
	GameWindow* gameWin = dynamic_cast<GameWindow*>(this->gameWin.get());
	assert(gameWin != nullptr and "current window doesn't support handling an event");

	// if (event is chat type)
	// 	gameWin->showChatMsg(event);
	// else
	gameWin->appendEvent(event);
	if (this->phase == GamePhase::GAME)
		gameWin->updateContentWindow();
}

void CLI::switchWindow(GamePhase newPhase)
{
	UI::switchWindow(newPhase);

	if (this->currentWindow and (newPhase != GamePhase::ERROR))
		this->currentWindow->clear();

	switch (newPhase)
	{
		case GamePhase::LOGIN:			this->currentWindow = this->loginWin.get(); break;
		case GamePhase::PLAYER_CREATE:	this->currentWindow = this->newPlayerWin.get(); break;
		case GamePhase::GAME:			this->currentWindow = this->gameWin.get(); break;
		case GamePhase::ERROR:			this->currentWindow = this->errorWin.get(); break;
		default:						break;
	}
	this->currentWindow->draw(this->height, this->width);
}

std::unique_ptr<UI> uiFactory()
{
	return std::make_unique<CLI>();
}
