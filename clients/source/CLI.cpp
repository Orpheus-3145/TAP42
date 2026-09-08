#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <signal.h>
#include <sys/ioctl.h>


CLI::CLI(int32_t commandFd) :
	GameInterface(commandFd),
	resizeFd{ioUtils::createSignalRedirectFd(SIGWINCH)}
{
	// this->currentTabIndex = 0UL;

	// adjust window size
	// printf("\033[8;%d;%dt", height, width);
	// fflush(stdout);

	// (void) height;
	// (void) width;

	::initscr();
	::cbreak();
	::noecho();

	this->frame = std::make_unique<BasicTab>(LINES, COLS, 0, 0, 0);
	this->frame->appendContent("insert some shit, 'quit' to close");
	
	this->commandTab = std::make_unique<InputTab>(LINES - 3, (COLS - 2) / 2, 2, 1, 0);
	this->commandTab->appendContent("this is where the input is shown");

	this->responseTab = std::make_unique<OutputTab>((LINES - 3) / 2, (COLS - 2) / 2, 2, (COLS - 2) / 2 + 1, 0);
	this->responseTab->appendContent("this is where responses are shown");

	this->eventTab = std::make_unique<OutputTab>((LINES - 3) / 2, (COLS - 2) / 2, (LINES - 3) / 2 + 2, (COLS - 2) / 2 + 1, 0);
	this->eventTab->appendContent("this is where events are shown");

	this->commandTab->refresh();

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

	LOG_INFO(LogContext::INTERFACE, "Setup for CLI done");
}

CLI::~CLI(void) noexcept
{
	::endwin();

	LOG_INFO(LogContext::INTERFACE, "CLI stopped");
}

void CLI::loop(void)
{
	this->refresh();
	while (this->KeepAlive == true)
	{
		int32_t input = this->commandTab->getChar();	// this is blocking

		this->dispatchUserInput(input);
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

// ioUtils::Pipe resizePipe = ioUtils::createPipe();
// signal_handler_fn = [resizePipe.in](int sig) {
// 	LOG_DEBUG(LogContext::INTERFACE, "(output) got resize callback");
// 	write(resizePipe.in, "x", 1);
//     // qui puoi usare catture, perché è uno std::function
// };
//
// std::signal(SIGWINCH, signal_handler_wrapper);
//
// signal(SIGWINCH, [](int fd) {
// });
//
// if (fds[0].revents & POLLIN)
// 	this->interface->handleUserInput();
//
// if (fds[1].revents & POLLIN)
// {
// 	char tmp;
// 	write(resizePipe.out, &tmp, 1);
//
// 	struct winsize ws;
// 	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
// 	resizeterm(ws.ws_row, ws.ws_col);
//
// 	LOG_DEBUG(LogContext::INTERFACE, "(input) got resize callback");
// 	this->interface->resize();
// }

void CLI::resize(void)
{
	// int32_t newHeight, newWidth;

	// getmaxyx(stdscr, newHeight, newWidth);

	// this->inputTabs[CLI::FRAME_TAB]->resize(newHeight, newWidth);
}

void CLI::dispatchUserInput(int32_t inputChar)
{
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

