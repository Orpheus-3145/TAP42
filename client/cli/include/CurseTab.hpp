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


static constexpr char const*	PROMPT = "-> ";
static constexpr const char		COMMAND_TERM = '\n';

static std::vector<const char*> HINTS
{
	"CONNECT",
	"LOOK",
	"MOVE",
	"WHO",
	"CHAT",
	"TAKE",
	"DROP",
	"INVENTORY",
	"TALK",
	"ATTACK",
	"STATUS",
	"QUEST",
	"QUESTS",
	"GROUP",
	"QUIT"
};

class BasicTab
{
	public:
		BasicTab(int32_t borderChar = -1, CurseWindow* parent = nullptr) : 
			borderChar{borderChar},
			parent{parent} {}
		BasicTab(BasicTab const& other) noexcept = delete;
		BasicTab& operator=(BasicTab const& other) noexcept = delete;
		BasicTab(BasicTab&&) noexcept;
		BasicTab& operator=(BasicTab&&) noexcept;

		virtual ~BasicTab(void) { this->clear(); }

		void printLine(std::string const& newContent) const noexcept;
		void refresh(void) const noexcept { ::wnoutrefresh(this->mainWin); }

		virtual void draw(int32_t h, int32_t w, int32_t y, int32_t x);
		virtual void appendContent(std::string const& newContent) noexcept;
		virtual void resize(int32_t h, int32_t w, int32_t y, int32_t x);

	protected:
		virtual void clear(void) noexcept;

		WINDOW* borderWin{nullptr};
		WINDOW* mainWin{nullptr};

		int32_t			borderChar;
		CurseWindow*	parent;

		std::vector<std::string> _state;
};

class InputTab : public BasicTab
{
	using InputDispatcher = std::unordered_map<int32_t,std::function<void()>>;

	public:
		using BasicTab::BasicTab;

		InputTab(int32_t forwardInputFd, int32_t borderChar = -1, CurseWindow* parent = nullptr);

		InputTab(InputTab&&) noexcept;
		InputTab& operator=(InputTab&&) noexcept;

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
		void appendContent(std::string const& newContent) noexcept override;

	protected:
		virtual void appendCharToInput(int32_t input);
		virtual void terminateInput(void);

		void updateHints(void) noexcept;
		void clearHints(void) noexcept;

		void overwriteLine(void) const noexcept;

		int32_t forwardInputFd;
		int32_t borderChar;

		InputDispatcher	dispatcher;

		std::deque<std::string>		history;
		std::vector<const char*>	hints;

		ssize_t			currentCommandIndex{-1L};
		ssize_t			currentSuggestedIndex{-1L};
		const int32_t	startX{static_cast<int32_t>(::strlen(PROMPT))};

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

		void draw(int32_t h, int32_t w, int32_t y, int32_t x) override;
		void appendContent(std::string const& newContent) noexcept override { (void) newContent; }

	protected:
		void terminateInput(void) override;
};

class OutputTab : public BasicTab
{
	public:
		using BasicTab::BasicTab;

		OutputTab(std::string const& title = "", int32_t borderChar = -1, CurseWindow* parent = nullptr) :
			BasicTab(borderChar, parent),
			title{title} {}

		OutputTab(OutputTab&&) noexcept;
		OutputTab& operator=(OutputTab&&) noexcept;

		void draw(int32_t h, int32_t w, int32_t y, int32_t x) override;
		void appendContent(std::string const& newContent) noexcept override;

	protected:
		virtual void clear(void) noexcept override;

		WINDOW* titleWin{nullptr};

		std::string 			title;
		std::deque<std::string> content;
		size_t					firstLineToPrintIndex{0UL};
};
