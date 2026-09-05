#include "UI.hpp"
#include "Exceptions.hpp"
#include "ClientHTTP.hpp"
#include "Utils.hpp"

#include <cstring>				// strerror, memchr, memeset, memmove
#include <string>
#include <cassert>


void BasicUI::setup(void)
{
	memset(this->readingBuffer, 0, Config::R_BUFF_SIZE);
	memset(this->writingBuffer, 0, Config::R_BUFF_SIZE);

	this->readingSize = 0UL;
	this->writingSize = 0UL;

	LOG_DEBUG("Setup for BasicUI interface done");
}

void BasicUI::sendDataToClient(int32_t clientSocket)
{
	assert(clientSocket != -1 and "invalid client socket");

	ioUtils::writeNonBlock(clientSocket, this->writingBuffer, this->writingSize);	// NB handle if returns -1
	LOG_DEBUG("Forwarding input to client: " + std::string(this->writingBuffer, this->writingSize));

	if (this->writingSize >= ::strlen(Config::QUIT) and !::strncmp(this->writingBuffer, Config::QUIT, ::strlen(Config::QUIT)))
		this->runLoop = false;
	memset(this->writingBuffer, 0, this->writingSize);
	this->writingSize = 0UL;
}

void BasicUI::readDataFromClient(int32_t clientSocket)
{
	assert(clientSocket != -1 and "invalid client socket");

	while(true)	// while loop because buffer could overflow
	{
		ssize_t n = ioUtils::readNonBlock(clientSocket, this->readingBuffer + this->readingSize, Config::R_BUFF_SIZE - this->readingSize);

		if (n > 0L)
		{
			LOG_DEBUG("Reading input from client: " + std::string(this->readingBuffer, this->readingSize));
			this->readingSize += n;
			this->parseInput();
			continue;
		}
		else if (n == -1L)		// client closed connection
		{
			LOG_WARN("Client unexpectedly terminated connection, closing session");
			this->runLoop = false;
		}
		break;
	}
}

void BasicUI::parseInput(void)
{
	char *startMsg = this->readingBuffer, *endMsg = nullptr;
	while (true)
	{
		endMsg = reinterpret_cast<char*>(::memchr(startMsg, Config::MSG_TERM, this->readingSize));
		if (endMsg == nullptr)
			break;
		
		ssize_t lenMsg = endMsg - startMsg;
		if (lenMsg < 2)
			throw CLIException("Bad server input: " + std::string(startMsg, lenMsg));

		if (!::strncmp(startMsg, "OK", 2) or !::strncmp(startMsg, "ERR", 3))
		{
			LOG_INFO("Got new response: " + std::string(startMsg, lenMsg));
			this->pendingResp = std::string(startMsg, lenMsg);		// NB check if there's already a response
		}
		else if (!::strncmp(startMsg, "EVT", 3))
		{
			LOG_INFO("Got new event: " + std::string(startMsg, lenMsg));
			this->eventQueue.push(std::string(startMsg, lenMsg));
		}
		else
			LOG_WARN("Unknown command: " + std::string(startMsg, lenMsg));

		startMsg += lenMsg + 1UL;
		this->readingSize -= lenMsg + 1UL;
	}
	if (this->readingSize > 0UL)
		::memmove(this->readingBuffer, startMsg, this->readingSize);
}


CommandLineUI::~CommandLineUI(void)
{
}

void CommandLineUI::setup(void)
{
	BasicUI::setup();

	// adjust window size
	printf("\033[8;%d;%dt", HEIGHT_WIN, WIDTH_WIN);
	fflush(stdout);

	initscr();
	cbreak();
	keypad(stdscr, TRUE);

	box(stdscr, 0, 0);
	mvaddstr(1, 1, "Type random shit, press 'enter' to send, 'quit' to close");
	::refresh();

	this->tabs.push_back(newwin(LINES - 3, (COLS - 2) / 2, 2, 1));
	box(this->tabs[0], 0, 0);
	mvwaddstr(this->tabs[0], this->currLineInput++, 1, "random shit [input]");
	
	this->tabs.push_back(newwin(LINES - 3, (COLS - 2) / 2, 2, (COLS - 2) / 2 + 1));
	box(this->tabs[1], 0, 0);
	mvwaddstr(this->tabs[1], this->currLineOutput++, 1, "random shit [output]");

	::wmove(this->tabs[0], this->currLineInput++, 1);
	::wrefresh(this->tabs[1]);
	::wrefresh(this->tabs[0]);

	LOG_DEBUG("Setup for CommandLine interface done");
}

void CommandLineUI::loop(int32_t clientSocket)		// NB move this loop in Game ?
{
	assert(clientSocket != -1 and "invalid client socket");

	LOG_INFO("Started UI client, using UNIX socket: " + std::to_string(clientSocket));
	this->runLoop = true;
	while(this->runLoop == true)
	{
		struct pollfd fds[2];
		// user input
		fds[0].fd = STDIN_FILENO;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		// client socket
		fds[1].fd = clientSocket;
		fds[1].events = POLLIN;
		fds[1].revents = 0;

		if (ioUtils::poll(fds, 2, -1) == -1)
		{
			if (errno == EINTR)
				continue;
			LOG_ERROR("Poll failed: " + std::string(strerror(errno)));
			throw CLIException("poll failed: " + std::string(strerror(errno)));
		}

		if (fds[0].revents & POLLIN)
			this->handleInputFromUser(clientSocket);

		if (fds[1].revents & POLLIN)
			this->handleInputFromClient(clientSocket);

		// client closed connection (because server did so) (POLLHUP) or got an error (POLLERR | POLLNVAL)
		if (fds[1].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			LOG_WARN("Client unexpectedly terminated connection, closing session");
			this->runLoop = false;
		}

		::wrefresh(this->tabs[1]);
		::wrefresh(this->tabs[0]);
	}
}

void CommandLineUI::stop(int32_t clientSocket) noexcept
{
	ioUtils::closeSocket(clientSocket);

	for (WINDOW* tab : tabs)
		::delwin(tab);
	::endwin();

	LOG_DEBUG("Game stopped");
}

void CommandLineUI::refreshTabs(void) const noexcept
{
	::wrefresh(this->tabs[1]);
	::wrefresh(this->tabs[0]);
}

void CommandLineUI::handleInputFromUser(int32_t clientSocket)
{
	int32_t inputChar = wgetch(this->tabs[0]);

	switch (inputChar)
	{
		case KEY_RESIZE:	// NB resize doesn't work
			LOG_DEBUG("Resize window callback");
			break;

		case Config::MSG_TERM:
			LOG_INFO("Got new command: " + std::string(this->writingBuffer, this->writingSize));
			this->sendDataToClient(clientSocket);		// NB map command into HTTP command
			::wmove(this->tabs[0], this->currLineInput++, 1);
			break;

		default:
			this->writingBuffer[this->writingSize++] = inputChar;
			break;
	}
	if (this->writingSize == BUFF_INPUT_SIZE)
	{
		LOG_ERROR("Input buffer overflow");
		throw CLIException("Input buffer overflow");
	}
}

void CommandLineUI::handleInputFromClient(int32_t clientSocket)
{
	this->readDataFromClient(clientSocket);
	if (this->pendingResp.length() > 0)
	{
		mvwprintw(this->tabs[1], this->currLineOutput++, 1, this->pendingResp.data());		// NB shouldn't allow multiple responses
		this->pendingResp.clear();
	}
	while (this->eventQueue.size() > 0UL)
	{
		mvwprintw(this->tabs[1], this->currLineOutput++, 1, this->eventQueue.front().data());
		this->eventQueue.pop();
	}
}
