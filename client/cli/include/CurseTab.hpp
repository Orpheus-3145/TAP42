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


enum class TextAlign : uint32_t
{
	LEFT_ALIGN = 0,
	MID_ALIGN = 1,
	RIGHT_ALIGN = 2,
};

class BasicTab
{
	using InputDispatcher = std::unordered_map<int32_t,std::function<void()>>;

	public:
		BasicTab(int32_t borderChar = -1, int32_t colorPair = -1, CurseWindow* parent = nullptr);
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
		CurseWindow*	parent;

		InputDispatcher	dispatcher{};
		bool			isActive{false};
};

class InputTab : public virtual BasicTab
{
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

		InputTab(InputTab&& other) noexcept;
		InputTab& operator=(InputTab&& other) = delete;

		virtual ~InputTab(void) { ::keypad(this->inputWin, false); }

		void deleteCharForward(void) noexcept;
		void deleteCharBack(void) noexcept;

		void moveCursorLeft(void) const noexcept;
		void moveCursorRight(void) const noexcept;

		void moveStartLine(void) const noexcept;
		void moveEndLine(void) const noexcept;

		void suggestHint(void) noexcept;
		void suggestPrevious(void) noexcept;
		void showPrevious(void) noexcept;

		void suggestNext(void) noexcept;
		void showNext(void) noexcept;

		void refresh(void) const noexcept override;
		void draw(int32_t h, int32_t w, int32_t y, int32_t x) override;
		void clear(void) noexcept override;
		void handleUserInput(void) override;

		void activate(void) override;
		void deactivate(void) override;

		void setChar(int32_t input);

		virtual void appendInputChar(int32_t input);
		virtual void terminateInput(void);

	protected:
		void updateHints(void) noexcept;
		void clearHints(void) noexcept;
		void writePromptLine(void) const noexcept;

		int32_t const forwardInputFd;

		std::vector<std::string> const	hints;
		std::string const				prompt;

		WINDOW*	inputWin{nullptr};

		std::deque<std::string>	inputHistory;
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

class OutputTab : public virtual BasicTab
{
	public:
		using BasicTab::BasicTab;

		OutputTab(std::string const& title = "", int32_t borderChar = -1, int32_t colorPair = -1, CurseWindow* parent = nullptr);

		OutputTab(OutputTab&& other) noexcept;
		OutputTab& operator=(OutputTab&& other) = delete;

		void refresh(void) const noexcept override;
		void draw(int32_t h, int32_t w, int32_t y, int32_t x) override;
		void clear(void) noexcept override;
		void resize(int32_t h, int32_t w, int32_t y, int32_t x) override;

		void activate(void) override;
		void deactivate(void) override;

		void appendContent(std::string const& newContent, TextAlign align = TextAlign::LEFT_ALIGN);
		void scrollContentUp(void) noexcept;
		void scrollContentDown(void) noexcept;

		void printLine(std::string const& newContent, TextAlign align = TextAlign::LEFT_ALIGN) const noexcept;
		void printLine(std::pair<std::string,TextAlign> const& content) const noexcept
			{ this->printLine(content.first, content.second); }

	protected:
		WINDOW*	titleWin{nullptr};
		WINDOW*	divLineWin{nullptr};
		WINDOW*	outputWin{nullptr};

		std::string const	title;

		std::deque<std::pair<std::string,TextAlign>> state;

		size_t		topLineScroll{0UL};
};

class InOutTab : public InputTab, public OutputTab
{
	public:
		InOutTab(
			int32_t forwardInputFd,
			std::vector<std::string> const& hints = std::vector<std::string>(),
			std::string const& prompt = "<?> ",
			std::string const& title = "",
			int32_t borderChar = -1,
			int32_t colorPair = -1,
			CurseWindow* parent = nullptr
		);

		InOutTab(InOutTab const& other) noexcept = delete;
		InOutTab& operator=(InOutTab const& other) noexcept = delete;
		InOutTab(InOutTab&& other) noexcept;
		InOutTab& operator=(InOutTab&& other) = delete;

		virtual ~InOutTab(void) { this->clear(); }

		void refresh(void) const noexcept override;
		void draw(int32_t h, int32_t w, int32_t y, int32_t x) override;
		void clear(void) noexcept override;

		void activate(void) override;
		void deactivate(void) override;

		void terminateInput(void) override;

	protected:
		WINDOW*	inputFrame{nullptr};
};
