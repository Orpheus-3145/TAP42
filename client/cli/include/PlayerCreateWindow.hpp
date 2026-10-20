#pragma once

#include <cstdint>

#include "NcWindow.hpp"


class PlayerCreateWindow : public NcWindow
{
	public:
		using NcWindow::NcWindow;

		PlayerCreateWindow(int32_t commandFd, UI* engine);

		virtual ~PlayerCreateWindow(void) noexcept { this->clear(); }

		void draw(int32_t height, int32_t width) override;

	protected:
		static constexpr size_t FRAME = 0UL;
		static constexpr size_t INFO = 1UL;
		static constexpr size_t USERNAME = 2UL;
		static constexpr size_t BACK = 3UL;
		static constexpr size_t CLOSE = 4UL;

		int32_t commandFd;
};
