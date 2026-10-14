#pragma once

#include <cstdint>
#include <string>
#include <functional>

#include "Window.hpp"
#include "UI.hpp"


class ErrorWindow : public Window
{
    Q_OBJECT

	public:
		using Window::Window;

		~ErrorWindow(void) noexcept {}

		void draw(void) override;
};

