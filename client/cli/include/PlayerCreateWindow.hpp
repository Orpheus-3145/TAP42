#pragma once

#include "TapWindow.hpp"

#include <cstdint>


class PlayerCreateWindow : public TapWindow
{
	public:
		PlayerCreateWindow(int32_t commandFd);

		virtual ~PlayerCreateWindow(void) noexcept { this->clear(); }

		void draw(int32_t height, int32_t width) override;
		void resize(int32_t height, int32_t width) override;

	protected:
		static constexpr size_t FRAME = 0UL;
		static constexpr size_t USERNAME = 1UL;

		int32_t commandFd;
};
