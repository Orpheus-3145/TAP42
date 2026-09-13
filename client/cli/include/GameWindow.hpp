#pragma once

#include <ncurses.h>
#include <memory>

#include "Config.hpp"
#include "CurseTab.hpp"


class GameWindow
{
	public:
		GameWindow(int32_t height, int32_t width, int32_t commandFd, int32_t chatFd);

		GameWindow(GameWindow const& other) = delete;
		GameWindow& operator=(GameWindow const& other) = delete;
		GameWindow(GameWindow&& other) = delete;
		GameWindow& operator=(GameWindow&& other) = delete;

		~GameWindow(void) noexcept { this->clear(); }

		void show(void);
		void clear(void) noexcept;
		void readInput(void);
		void resize(int32_t height, int32_t width);
		void refresh(void) noexcept { ::doupdate(); }

		void handleResponse(std::string const& response) noexcept;
		void handleEvent(std::string const& event) noexcept;

	private:
		int32_t height;
		int32_t width;

		int32_t commandFd;
		int32_t chatFd;

		// NB add deque and indexes instead of single tabs

		std::unique_ptr<OutputTab>	frame;
		std::unique_ptr<InputTab>	commandTab;
		std::unique_ptr<OutputTab>	responseTab, eventTab;
};