#pragma once

#include <ncurses.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <deque>
#include <cstring>

#include <Config.hpp>
#include <CurseWindow.hpp>


class BasicTab
{
	public:
		BasicTab(int32_t borderChar = -1, int32_t colorPair = -1, CurseWindow* parent = nullptr) : 
			borderChar{borderChar},
			colorPair{colorPair},
			parent{parent} {
				if (::has_colors() == false) this->colorPair = -1;
			}
		BasicTab(BasicTab const& other) noexcept = delete;
		BasicTab& operator=(BasicTab const& other) noexcept = delete;
		BasicTab(BasicTab&&) noexcept;
		BasicTab& operator=(BasicTab&&) noexcept;

		virtual ~BasicTab(void) { this->clear(); }

		virtual void refresh(void) const noexcept { ::wnoutrefresh(this->mainWin); }

		virtual void draw(int32_t h, int32_t w, int32_t y, int32_t x);
		virtual void resize(int32_t h, int32_t w, int32_t y, int32_t x);

		virtual void appendContent(std::string const& newContent);
		virtual void printLine(std::string const& newContent) const noexcept;

		virtual void clear(void) noexcept;
		
	protected:

		WINDOW* borderWin{nullptr};
		WINDOW* mainWin{nullptr};

		int32_t			borderChar, colorPair;
		CurseWindow*	parent;

		std::deque<std::string> _state;
};

class InputTab : public BasicTab
{
	using InputDispatcher = std::unordered_map<int32_t,std::function<void()>>;

	public:
		using BasicTab::BasicTab;

		InputTab(
			int32_t forwardInputFd,
			std::vector<std::string> const& hints = std::vector<std::string>(),
			std::string const& prompt = "<?> ",
			int32_t colorPair = -1,
			int32_t borderChar = -1,
			CurseWindow* parent = nullptr
		);

		InputTab(InputTab&&) noexcept;
		InputTab& operator=(InputTab&&) = delete;

		virtual ~InputTab(void) { ::keypad(this->mainWin, false); }

		void handleUserInput(void);

		void deleteCharForward(void) noexcept;
		void deleteCharBack(void) noexcept;

		void moveCursorLeft(void) const noexcept;
		void moveCursorRight(void) const noexcept;

		void moveStartLine(void) const noexcept;
		void moveEndLine(void) const noexcept;

		void setChar(int32_t input);

		void suggestPrevious(void) noexcept;
		void showPrevious(void) noexcept;

		void suggestNext(void) noexcept;
		void showNext(void) noexcept;

		void draw(int32_t h, int32_t w, int32_t y, int32_t x) override;
		void appendContent(std::string const& newContent) override;

	protected:
		virtual void appendCharToInput(int32_t input);
		virtual void terminateInput(void);

		void updateHints(void) noexcept;
		void clearHints(void) noexcept;
		void writePromptLine(void) const noexcept;

		int32_t const forwardInputFd;

		std::vector<std::string> const	hints;
		std::string const				prompt;

		InputDispatcher	dispatcher;

		std::deque<std::string>	history;
		std::vector<uint32_t>	suggestedHintIndexes;

		ssize_t			currentCommandIndex{-1L};
		ssize_t			currentSuggestedIndex{-1L};
		int32_t const	startX{static_cast<int32_t>(this->prompt.size())};

		size_t	bufferSize{0UL};
		char	commandBuffer[Config::CMD_BUFFER_SIZE];

		size_t	tmpBufferSize{0UL};
		char	tmpCommandBuffer[Config::CMD_BUFFER_SIZE];

		bool	autocompleteMode{false};
};

class SingleInputTab : public InputTab
{
	public:
		using InputTab::InputTab;

		void appendContent(std::string const& newContent) override { (void) newContent; }

	protected:
		void terminateInput(void) override;
};

class OutputTab : public BasicTab
{
	public:
		using BasicTab::BasicTab;

		OutputTab(std::string const& title = "", int32_t borderChar = -1, int32_t colorPair = -1, CurseWindow* parent = nullptr) :
			BasicTab(borderChar, colorPair, parent),
			title{title} {}

		OutputTab(OutputTab&&) noexcept;
		OutputTab& operator=(OutputTab&&) noexcept;

		void draw(int32_t h, int32_t w, int32_t y, int32_t x) override;

	protected:
		virtual void clear(void) noexcept override;

		WINDOW*		titleWin{nullptr};
		std::string	title;
};
