#pragma once

#include <ncurses.h>
#include <memory>

#include "CurseWindow.hpp"
#include "CurseTab.hpp"
#include "Config.hpp"


class GameWindow : public CurseWindow
{
	public:
		using CurseWindow::CurseWindow;

		GameWindow(int32_t height, int32_t width, int32_t commandFd, int32_t messageFd);

		virtual ~GameWindow(void) noexcept { this->clear(); }

		void handleResponse(std::string const& response) noexcept;
		void handleEvent(std::string const& event) noexcept;

		void show(void) override;
		void clear(void) noexcept override;
		void readInput(void) override;
		void resize(int32_t height, int32_t width) override;
		void switchNextTab(void) noexcept override;
		void switchPreviousTab(void) noexcept override { this->switchNextTab(); }

	protected:
		int32_t commandFd;
		int32_t messageFd;

		std::unique_ptr<OutputTab>	frame,
									infoTab,
									responseTab,
									eventTab,
									chatTab,
									heightTBATab;

		std::unique_ptr<SingleInputTab>	commandTab, messageTab;
};
