#pragma once

#include <ncurses.h>
#include <unordered_map>
#include <functional>
#include <memory>

#include "Config.hpp"
#include "Utils.hpp"
#include "Game.hpp"
#include "CurseTab.hpp"


class CLI : public GameInterface
{
	// using TabVector = std::vector<std::unique_ptr<BasicTab>>;

	public:
		using GameInterface::GameInterface;
		CLI(int32_t commandFd);

		~CLI(void) noexcept;
		
		void loop(void) override;
		void handleResponse(std::string const& response) override;
		void handleEvent(std::string const& event) override;
		void forwardCommandToServer(std::string const& command) override;

	private:
		void resize(void) override;
		void refresh(void) noexcept override { ::doupdate(); }

		// InputTab& getCurrentTab(void);

		// void switchForwardTab(void) noexcept;
		// void switchBackwardTab(void) noexcept;
		// BasicTab* getCurrentTab(void) noexcept;
		// void setCurrentTab(size_t currentITabIndex) noexcept;

		void dispatchUserInput(int inputChar);

		static constexpr size_t N_TABS = 3;
		static constexpr size_t FRAME_TAB = 0;
		static constexpr size_t CMD_TAB = 1;
		static constexpr size_t OUTPUT_TAB = 2;

		std::mutex respMutex, eventMutex;

		ioUtils::Pipe resizePipe;

		std::unique_ptr<BasicTab>	frame;
		std::unique_ptr<InputTab>	commandTab;
		std::unique_ptr<OutputTab>	responseTab, eventTab;

		// TabVector	tabs;
		// size_t		currentTabIndex{0UL};
		// size_t					currentInTabIndex{0UL};
		// size_t					currentOutTabIndex{0UL};

		std::unordered_map<int32_t,std::function<void()>>	_dispatcher;
};