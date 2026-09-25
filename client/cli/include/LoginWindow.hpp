#pragma once

#include "TapWindow.hpp"

#include <cstdint>


class LoginWindow : public TapWindow
{
	public:
		LoginWindow(int32_t commandFd);

		virtual ~LoginWindow(void) noexcept { this->clear(); }

		void draw(int32_t height, int32_t width) override;

	protected:
		static constexpr size_t FRAME = 0UL;
		static constexpr size_t INFO = 1UL;
		static constexpr size_t USERNAME = 2UL;

		int32_t commandFd;
};
