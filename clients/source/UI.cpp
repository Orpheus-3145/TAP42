#include "UI.hpp"
#include "Exceptions.hpp"
#include "ClientHTTP.hpp"

#include <string>
#include <cstring>
#include <cassert>


CommandLineUI::~CommandLineUI(void)
{
	if (this->tabs.empty() == false)
		this->clear();
}

void CommandLineUI::setup(void)
{
	this->commandLength = 0L;
	this->currLineInput = 1UL;
	this->currLineOutput = 1UL;

	// adjust window size
	printf("\033[8;%d;%dt", HEIGHT_WIN, WIDTH_WIN);
	fflush(stdout);

	initscr();
	cbreak();
	keypad(stdscr, TRUE);

	box(stdscr, 0, 0);
	mvaddstr(1, 1, "Type random shit, press 'enter' to send, 'quit' to close");

	this->tabs.push_back(newwin(LINES - 3, (COLS - 2) / 2, 2, 1));
	box(this->tabs[0], 0, 0);
	mvwaddstr(this->tabs[0], this->currLineInput++, 1, "random shit [input]");
	
	this->tabs.push_back(newwin(LINES - 3, (COLS - 2) / 2, 2, (COLS - 2) / 2 + 1));
	box(this->tabs[1], 0, 0);
	mvwaddstr(this->tabs[1], this->currLineOutput++, 1, "random shit [output]");

	::wmove(this->tabs[0], this->currLineInput++, 1);

	::wnoutrefresh(stdscr);
	::wnoutrefresh(this->tabs[1]);
	::wnoutrefresh(this->tabs[0]);
	LOG_DEBUG(LogContext::UI, "Setup for CommandLine interface done");
}

void CommandLineUI::clear(void) noexcept
{
	for (WINDOW*& tab : this->tabs)
		::delwin(tab);
	this->tabs.clear();
	::endwin();

	LOG_INFO(LogContext::UI, "UI stopped");
}

void CommandLineUI::handleUserInput(void)
{
	int32_t inputChar = wgetch(this->tabs[0]);

	if (this->commandLength == Config::BUFF_SIZE)
		throw BufferOverflowException("Command buffer overflow");

	switch (inputChar)
	{
		case KEY_RESIZE:	// NB resize doesn't work
			LOG_DEBUG(LogContext::UI, "Resize window callback");
			break;

		case Config::COMMAND_TERM:
			LOG_INFO(LogContext::UI, "Got new command: " + std::string(this->commandBuffer, this->commandLength));
			ioUtils::write(this->commandPipe.in, this->commandBuffer, this->commandLength);
			this->commandLength = 0UL;
			::wmove(this->tabs[0], this->currLineInput++, 1);
			break;

		default:
			this->commandBuffer[this->commandLength++] = inputChar;
			break;
	}
	::wnoutrefresh(this->tabs[0]);
}

void CommandLineUI::handleResponse(std::string const& response)
{
	mvwprintw(this->tabs[1], this->currLineOutput++, 1, response.data());
	::wnoutrefresh(this->tabs[1]);
	::wnoutrefresh(this->tabs[0]);
}

void CommandLineUI::handleEvent(std::string const& event)
{
	mvwprintw(this->tabs[1], this->currLineOutput++, 1, event.data());
	::wnoutrefresh(this->tabs[1]);
	::wnoutrefresh(this->tabs[0]);
}
