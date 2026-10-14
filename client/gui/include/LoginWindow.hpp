#pragma once

#include <cstdint>

#include "Window.hpp"


class LoginWindow : public Window
{
    Q_OBJECT

	public:
		using Window::Window;

		~LoginWindow(void) noexcept {}

		void draw(void) override;
};