#pragma once

#include <ncurses.h>
#include <string>
#include <functional>
#include <deque>
#include <cstdint>
#include <optional>
#include <chrono>
// #include <iomanip>

#include "BasicTab.hpp"
#include "TapWindow.hpp"


enum class TextAlign : uint32_t
{
	LEFT = 0,
	MID = 1,
	RIGHT = 2,
};

struct TabMessage
{
	using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration>;

	std::string					content;			// NB handle multi-line messages
	TextAlign					align;
	uint32_t					attrs;
	std::optional<TimePoint>	timestamp;

	explicit TabMessage(std::string const content, TextAlign align = TextAlign::LEFT, uint32_t attrs = A_NORMAL, bool saveTimestamp = true) :
		content{content},
		align{align},
		attrs{attrs} { if (saveTimestamp) this->timestamp = std::chrono::system_clock::now(); }

	TabMessage(void) = delete;
	
	TabMessage(TabMessage const& other) noexcept = default;
	TabMessage& operator=(TabMessage const& other) noexcept = default;
	TabMessage(TabMessage&& other) noexcept = default;
	TabMessage& operator=(TabMessage&& other) noexcept = default;

	~TabMessage(void) noexcept = default;
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
		void updateContent(void) const noexcept override;

		void activate(void) override;
		void deactivate(void) override;

		void appendContent(std::string const& newContent, TextAlign align = TextAlign::LEFT, uint32_t attrs = A_NORMAL, bool saveTimestamp = true);
		void scrollContentUp(void) noexcept;
		void scrollContentDown(void) noexcept;

		virtual void	printLine(TabMessage const& content) const noexcept;
		void			printLine(std::string const& newContent) const noexcept { this->printLine(TabMessage{newContent}); }

		void clearContent(void) noexcept { this->state.clear(); this->topLineScroll = 0UL; }

	protected:
		WINDOW*	titleWin{nullptr};
		WINDOW*	divLineWin{nullptr};
		WINDOW*	outputWin{nullptr};

		std::string const	title;

		std::deque<TabMessage> state;

		size_t		topLineScroll{0UL};
};
