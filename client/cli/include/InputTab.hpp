#pragma once

#include <ncurses.h>
#include <deque>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

#include <Config.hpp>
#include <BasicTab.hpp>
#include <TapWindow.hpp>


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
			TapWindow* parent = nullptr
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
