#pragma once

#include <ncurses.h>
#include <vector>
#include <array>
#include <string>
#include <mutex>
#include <deque>
#include <cstring>


static constexpr char const*	PROMPT = "-> ";
static constexpr char const*	QUIT = "quit";
static constexpr const char		COMMAND_TERM = '\n';
static constexpr const size_t	CMD_BUFFER_SIZE = 128UL;

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
		BasicTab(void) : BasicTab::BasicTab(90, 40, 0, 0) {}
		BasicTab(int32_t h, int32_t w, int32_t y, int32_t x, int32_t borderChar = -1);

		BasicTab(BasicTab const& other) noexcept = delete;
		BasicTab& operator=(BasicTab const& other) noexcept = delete;
		BasicTab(BasicTab&&) noexcept;
		BasicTab& operator=(BasicTab&&) noexcept;

		virtual ~BasicTab(void);

		virtual void appendContent(std::string const& newContent) noexcept;
		virtual void refresh(void) const noexcept { ::wnoutrefresh(this->main); }
		virtual void resize(int32_t newHeight, int32_t newWidth, int32_t newY = -1, int32_t newX = -1);

		void printLine(std::string const& newContent) const noexcept;
	
	protected:
		WINDOW* border{nullptr};
		WINDOW* main{nullptr};

		std::vector<std::string> _state;
};

class InputTab : public BasicTab
{
	using HistoryCommands = std::deque<std::pair<size_t,std::array<char,CMD_BUFFER_SIZE>>>;		// ugly, store it as dyn ptrs?

	public:
		using BasicTab::BasicTab;

		InputTab(void);
		InputTab(int32_t h, int32_t w, int32_t y, int32_t x, int32_t borderChar = -1);

		InputTab(InputTab&&) noexcept;
		InputTab& operator=(InputTab&&) noexcept;

		void deleteCharForward(void) noexcept;
		void deleteCharBack(void) noexcept;

		void moveCursorLeft(void) const noexcept;
		void moveCursorRight(void) const noexcept;

		void setChar(int32_t input);
		std::string getLastInput(void) const noexcept;
		int32_t getChar(void) const noexcept;

		void suggestNextHint(void) noexcept;
		void suggestPastHint(void) noexcept;

		void showPrevious(void) noexcept;
		void showFollowing(void) noexcept;

		void appendContent(std::string const& newContent) noexcept override;

	private:
		void updateHints(void) noexcept;
		void clearHints(void) noexcept;

		void showInput(void) const noexcept;

		HistoryCommands				history;
		std::vector<const char*>	hints;

		ssize_t			currentCommandIndex{-1L};
		ssize_t			currentSuggestedIndex{-1L};
		const int32_t	startX{::strlen(PROMPT)};

		size_t	bufferSize{0UL};
		char	commandBuffer[CMD_BUFFER_SIZE];

		size_t	tmpBufferSize{0UL};
		char	tmpCommandBuffer[CMD_BUFFER_SIZE];

		bool autocompleteMode{false};
};

class OutputTab : public BasicTab
{
	public:
		using BasicTab::BasicTab;

		OutputTab(OutputTab&&) noexcept;
		OutputTab& operator=(OutputTab&&) noexcept;

		void appendContent(std::string const& newContent) noexcept override;

	private:
		std::deque<std::string> content;
		size_t					firstLineToPrintIndex{0UL};
};