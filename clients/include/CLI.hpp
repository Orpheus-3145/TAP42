#pragma once

#include <ncurses.h>
#include <vector>
#include <unordered_map>
#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <cstring>

#include "Config.hpp"
#include "Utils.hpp"
#include "Game.hpp"


static constexpr char const*	QUIT = "quit";
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
	using HistoryCommands = std::deque<std::pair<size_t,std::array<char,Config::BUFF_SIZE>>>;		// ugly, store it as dyn ptrs?

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
		const int32_t	startX{::strlen(Config::PROMPT)};

		size_t	bufferSize{0UL};
		char	commandBuffer[Config::BUFF_SIZE];

		size_t	tmpBufferSize{0UL};
		char	tmpCommandBuffer[Config::BUFF_SIZE];

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


class CLI : public GameInterface
{
	// using TabVector = std::vector<std::unique_ptr<BasicTab>>;

	public:
		using GameInterface::GameInterface;
		CLI(int32_t commandFd);

		~CLI(void) noexcept;
		
		void loop(void) override;
		void handleResponse(std::string const& response) override;
		void handleEvent(std::string const& event) override;
		void forwardCommandToServer(std::string const& command) override;

	private:
		void resize(void) override;
		void refresh(void) noexcept override { ::doupdate(); }

		// InputTab& getCurrentTab(void);

		// void switchForwardTab(void) noexcept;
		// void switchBackwardTab(void) noexcept;
		// BasicTab* getCurrentTab(void) noexcept;
		// void setCurrentTab(size_t currentITabIndex) noexcept;

		void dispatchUserInput(int inputChar);

		static constexpr size_t N_TABS = 3;
		static constexpr size_t FRAME_TAB = 0;
		static constexpr size_t CMD_TAB = 1;
		static constexpr size_t OUTPUT_TAB = 2;

		std::mutex respMutex, eventMutex;

		ioUtils::Pipe resizePipe;

		std::unique_ptr<BasicTab>	frame;
		std::unique_ptr<InputTab>	commandTab;
		std::unique_ptr<OutputTab>	responseTab, eventTab;

		// TabVector	tabs;
		// size_t		currentTabIndex{0UL};
		// size_t					currentInTabIndex{0UL};
		// size_t					currentOutTabIndex{0UL};

		std::unordered_map<int32_t,std::function<void()>>	_dispatcher;
};