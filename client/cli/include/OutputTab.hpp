#pragma once

#include <ncurses.h>
#include <string>
#include <functional>
#include <deque>
#include <cstdint>

#include <BasicTab.hpp>
#include <TapWindow.hpp>


enum class TextAlign : uint32_t
{
	LEFT_ALIGN = 0,
	MID_ALIGN = 1,
	RIGHT_ALIGN = 2,
};

class OutputTab : public virtual BasicTab
{
	public:
		using BasicTab::BasicTab;

		OutputTab(std::string const& title = "", int32_t borderChar = -1, int32_t colorPair = -1, TapWindow* parent = nullptr);

		OutputTab(OutputTab&& other) noexcept;
		OutputTab& operator=(OutputTab&& other) = delete;

		void refresh(void) const noexcept override;
		void draw(int32_t h, int32_t w, int32_t y, int32_t x) override;
		void clear(void) noexcept override;
		void resize(int32_t h, int32_t w, int32_t y, int32_t x) override;
		void updateContent(void) const noexcept override;

		void activate(void) override;
		void deactivate(void) override;

		void appendContent(std::string const& newContent, TextAlign align = TextAlign::LEFT_ALIGN);
		void scrollContentUp(void) noexcept;
		void scrollContentDown(void) noexcept;

		void printLine(std::string const& newContent, TextAlign align = TextAlign::LEFT_ALIGN) const noexcept;
		void printLine(std::pair<std::string,TextAlign> const& content) const noexcept
			{ this->printLine(content.first, content.second); }

		void clearContent(void) noexcept { this->state.clear(); }

	protected:
		WINDOW*	titleWin{nullptr};
		WINDOW*	divLineWin{nullptr};
		WINDOW*	outputWin{nullptr};

		std::string const	title;

		std::deque<std::pair<std::string,TextAlign>> state;

		size_t		topLineScroll{0UL};
};
