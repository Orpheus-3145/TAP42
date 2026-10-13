#pragma once

#include <cstdint>

#include "Window.hpp"


class LoginWindow : public Window
{
    Q_OBJECT

	public:
		using Window::Window;

		~LoginWindow(void) noexcept {}

	private:
		void draw(int32_t height, int32_t width) override;
};