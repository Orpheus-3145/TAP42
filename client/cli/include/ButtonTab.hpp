#pragma once

#include <ncurses.h>
#include <unordered_map>
#include <string>
#include <functional>

#include <BasicTab.hpp>
#include <TapWindow.hpp>


class ButtonTab :  public BasicTab
{
	public:
		using BasicTab::BasicTab;

		ButtonTab(std::string const& content, std::function<void()> action, int32_t colorPair = -1, TapWindow* parent = nullptr);

		ButtonTab(ButtonTab&& other) noexcept;
		ButtonTab& operator=(ButtonTab&& other) = delete;

		virtual ~ButtonTab(void) { this->clear(); }

		void draw(int32_t h, int32_t w, int32_t y, int32_t x) override;
		void refresh(void) const noexcept override;
		void clear(void) noexcept override;

		void activate(void) override;
		void deactivate(void) override;

		size_t getWidth(void) const noexcept { return this->content.size() + 2; }

	protected:
		std::string				content;
		std::function<void()>	action;

		WINDOW*	btnWin{nullptr};
};
