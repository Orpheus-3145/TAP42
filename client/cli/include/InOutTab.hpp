#pragma once

#include <ncurses.h>
#include <vector>
#include <string>

#include <InputTab.hpp>
#include <OutputTab.hpp>
#include <TapWindow.hpp>


class InOutTab : public InputTab, public OutputTab
{
	public:
		InOutTab(
			int32_t forwardInputFd,
			std::vector<std::string> const& hints = std::vector<std::string>(),
			bool hideInput = false,
			std::string const& prompt = "<?> ",
			std::string const& title = "",
			int32_t borderChar = -1,
			int32_t colorPair = -1,
			TapWindow* parent = nullptr
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

		bool	hideInput;
};
