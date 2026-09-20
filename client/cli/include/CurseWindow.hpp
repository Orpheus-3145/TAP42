#pragma once

#include <ncurses.h>
#include <memory>

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
		virtual void clear(void) noexcept = 0;
		virtual void readInput(void) = 0;
		virtual void resize(int32_t height, int32_t width) = 0;
		virtual void switchInputTab(void) noexcept = 0;
		virtual void scrollTab(bool goingUp) noexcept = 0;

		void refresh(void) noexcept { ::doupdate(); }

	protected:
		int32_t height{0};
		int32_t width{0};

		BasicTab* currentTab{nullptr};
};
