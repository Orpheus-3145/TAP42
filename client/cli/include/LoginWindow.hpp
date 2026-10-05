#pragma once

#include <cstdint>

#include "TapWindow.hpp"


class LoginWindow : public TapWindow
{
	public:
		using TapWindow::TapWindow;

		LoginWindow(int32_t commandFd, UI* engine);

		virtual ~LoginWindow(void) noexcept { this->clear(); }

		void draw(int32_t height, int32_t width) override;

	protected:
		static constexpr size_t FRAME = 0UL;
		static constexpr size_t INFO = 1UL;
		static constexpr size_t USERNAME = 2UL;
		static constexpr size_t CREATE_PLAYER = 3UL;
		static constexpr size_t CLOSE = 4UL;

		int32_t commandFd;
};
