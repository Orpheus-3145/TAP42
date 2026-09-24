#pragma once

#include <ncurses.h>
#include <unordered_map>
#include <string>
#include <functional>

#include <TapWindow.hpp>


class BasicTab
{
	using InputDispatcher = std::unordered_map<int32_t,std::function<void()>>;

	public:
		BasicTab(int32_t borderChar = -1, int32_t colorPair = -1, TapWindow* parent = nullptr);

		BasicTab(BasicTab const& other) noexcept = delete;
		BasicTab& operator=(BasicTab const& other) noexcept = delete;
		BasicTab(BasicTab&& other) noexcept;
		BasicTab& operator=(BasicTab&& other) noexcept;

		virtual ~BasicTab(void) { this->clear(); }

		virtual void refresh(void) const noexcept;
		virtual void draw(int32_t h, int32_t w, int32_t y, int32_t x);
		virtual void resize(int32_t h, int32_t w, int32_t y, int32_t x);
		virtual void clear(void) noexcept;
		virtual void handleUserInput(void);

		virtual void activate(void) { this->isActive = true; }
		virtual void deactivate(void) { this->isActive = false; }

	protected:
		WINDOW* borderWin{nullptr};

		int32_t			borderChar, colorPair;
		TapWindow*	parent;

		InputDispatcher	dispatcher{};
		bool			isActive{false};
};
