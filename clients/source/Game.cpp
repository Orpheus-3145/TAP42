#include <cassert>
#include <iostream>
#include <readline/readline.h>
#include <readline/history.h>
#include <ncurses.h>

#include "Game.hpp"
#include "Exceptions.hpp"


Game::Game(void)
{
	this->clientHTTP = std::make_unique<ClientHTTP>(printMutex);
	this->interface = std::make_unique<CLI>();
}

Game::~Game(void)
{
}

void Game::connectToServer(std::string const& host, uint32_t port)
{
	assert(this->clientHTTP and "client not existing");

	this->clientHTTP->connect(host, port);
}

void Game::gameLoop(void)
{
	this->printInfo();
	this->clientHTTP->startWorker();
	this->interface->loop();

	char* userInput = nullptr;
	while (true)
	{
		if (userInput == nullptr)
		{
			userInput = readline(PROMPT);
			if (userInput == nullptr)
				{/* ctrl+D, close app */}
			else if (userInput[0])
				add_history(userInput);
	
			if (std::string(userInput) == "end")
				break;
	
			this->printAsync("sent request: " + std::string(userInput));
			this->clientHTTP->sendCommandToServer(userInput);
			free(userInput);
		}

		if (clientHTTP->hasNewResponse())
		{
			this->printAsync("got response: " + clientHTTP->consumeResponse());
			userInput = nullptr;
		}
		while (clientHTTP->hasNewEvent())
			this->printAsync("got event: " + clientHTTP->popEvent());
	}
}

void Game::printInfo(void) noexcept
{
	std::string content = "welcome to " + std::string(GAME_NAME) + "\n" + \
		"enter input, type 'end' to close" + "\n\n";
	this->printAsync(content);
}

void Game::printAsync(std::string const& content) noexcept
{
	std::lock_guard<std::mutex> lock(this->printMutex);
	std::cout << content << std::endl;
}
