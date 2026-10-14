#pragma once

#include <cstdint>

#include "Window.hpp"


class PlayerCreateWindow : public Window
{
    Q_OBJECT

	public:
		using Window::Window;

		~PlayerCreateWindow(void) noexcept {}

		void draw(void) override;
};
