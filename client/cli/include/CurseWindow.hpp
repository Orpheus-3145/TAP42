#pragma once

#include <ncurses.h>
#include <memory>
#include <vector>
#include <set>

#include "Config.hpp"


class BasicTab;

class CurseWindow
{
	public:
		CurseWindow(void) noexcept = default;

		CurseWindow(CurseWindow const& other) = delete;
		CurseWindow& operator=(CurseWindow const& other) = delete;
		CurseWindow(CurseWindow&& other) = delete;
		CurseWindow& operator=(CurseWindow&& other) = delete;

		virtual ~CurseWindow(void) noexcept;

		virtual void draw(int32_t height, int32_t width) = 0;
		virtual void resize(int32_t height, int32_t width) = 0;
		virtual void clear(void) noexcept;

		virtual void	switchActiveTab(int32_t index = -1);
		BasicTab*		getActiveTab(void) { return this->tabs.at(this->activeTabIndex).get(); }

		void refresh(void) noexcept { ::doupdate(); }

	protected:
		std::vector<std::unique_ptr<BasicTab>>	tabs{};
		std::set<size_t>						tabsToSkip{};

		int32_t height{0};
		int32_t width{0};
		int32_t activeTabIndex{0};
};
