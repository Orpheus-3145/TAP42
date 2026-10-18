#pragma once

#include <cstdint>

#include "QtWindow.hpp"


class LoginWindow : public QtWindow
{
    Q_OBJECT

	public:
		using QtWindow::QtWindow;

		~LoginWindow(void) noexcept {}

		void draw(void) override;
};