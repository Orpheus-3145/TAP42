#include "CLI.hpp"

#include<cstring>
#include<string>


CLI::CLI(void)
{
	::initscr();
	::cbreak();
	::noecho();
	::keypad(stdscr, TRUE);
}

CLI::~CLI(void)
{
	::endwin();
}

void CLI::loop(void)
{
}
