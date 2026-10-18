#pragma once

#include <cstdint>
#include <string>
#include <functional>

#include "QtWindow.hpp"
#include "UI.hpp"


class ErrorWindow : public QtWindow
{
    Q_OBJECT

	public:
		using QtWindow::QtWindow;

		~ErrorWindow(void) noexcept {}

		void draw(void) override;
};

