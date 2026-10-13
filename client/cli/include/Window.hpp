#pragma once

#include <ncurses.h>
#include <memory>
#include <map>
#include <set>
#include <cstdint>

#include "UI.hpp"


class BasicTab;
class UI;

class Window
{
	public:
		Window(UI* engine) noexcept;

		Window(Window const& other) = delete;
		Window& operator=(Window const& other) = delete;
		Window(Window&& other) = delete;
		Window& operator=(Window&& other) = delete;

		virtual ~Window(void) noexcept;

		virtual void draw(int32_t height, int32_t width) = 0;
		virtual void resize(int32_t height, int32_t width);
		virtual void clear(void) noexcept;
		virtual void updateContentWindow(void) noexcept;
		virtual void handleUserInput(void);

		void		switchActiveTab(int32_t index = -1);
		BasicTab*	getActiveTab(void) { return this->tabs.at(this->activeTabIndex).get(); }

		void refresh(void) const noexcept { ::doupdate(); }

	protected:
		UI* const engine;

		std::map<size_t,std::unique_ptr<BasicTab>>	tabs{};
		std::set<size_t>							tabsToSkip{};

		int32_t height{0};
		int32_t width{0};
		int32_t activeTabIndex{0};
};
