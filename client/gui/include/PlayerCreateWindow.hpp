#pragma once

#include <cstdint>

#include "QtWindow.hpp"


class PlayerCreateWindow : public QtWindow
{
    Q_OBJECT

	public:
		using QtWindow::QtWindow;

		~PlayerCreateWindow(void) noexcept {}

		void draw(void) override;
};
