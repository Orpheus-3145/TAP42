#pragma once

#include <ncurses.h>
#include <memory>

#include "Config.hpp"


class BasicTab;

class CurseWindow
{
	public:
		CurseWindow(int32_t height, int32_t width) :
			height{height},
			width{width} {}

		CurseWindow(CurseWindow const& other) = delete;
		CurseWindow& operator=(CurseWindow const& other) = delete;
		CurseWindow(CurseWindow&& other) = delete;
		CurseWindow& operator=(CurseWindow&& other) = delete;

		virtual ~CurseWindow(void) noexcept;

		virtual void show(void) = 0;
		virtual void clear(void) noexcept = 0;
		virtual void readInput(void) = 0;
		virtual void resize(int32_t height, int32_t width) = 0;
		virtual void switchNextTab(void) noexcept = 0;
		virtual void switchPreviousTab(void) noexcept = 0;

		void refresh(void) noexcept { ::doupdate(); }

	protected:
		int32_t height;
		int32_t width;

		BasicTab* currentTab{nullptr};
};
