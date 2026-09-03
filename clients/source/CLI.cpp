#include "CLI.hpp"

#include<cstring>
#include<string>


CLI::CLI(void)
{
	initscr();
	keypad(stdscr, TRUE);
}

CLI::~CLI(void)
{
	endwin();
}

void CLI::loop(void)
{
	box(stdscr, 0, 0);
	mvaddstr(1, 1, "Type random shit, press enter to send, 'q' to close");
	refresh();

	char buffer[1000];
	memset(buffer, 0, 1000);

	uint32_t currentLine = 2U;

	do
	{
		mvgetstr(currentLine++, 1, buffer);
		if (!strcmp(buffer, "q"))
			break;

		mvprintw(currentLine++, 1, "You typed this shit: %s", buffer);
		currentLine++;
		refresh();
	} while (true);
}
