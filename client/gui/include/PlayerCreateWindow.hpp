#pragma once

#include <cstdint>
#include <QLineEdit>

#include "QtWindow.hpp"


class PlayerCreateWindow : public QtWindow
{
    Q_OBJECT

	public:
		using QtWindow::QtWindow;

		PlayerCreateWindow(UI* engine);

		~PlayerCreateWindow(void) noexcept {}

	private:
		static constexpr int32_t HEIGHT = 230;
		static constexpr int32_t WIDTH = 250;

		QLineEdit* inputUsername{nullptr};
};
