#pragma once

#include <cstdint>
#include <QLineEdit>

#include "QtWindow.hpp"


class LoginWindow : public QtWindow
{
    Q_OBJECT

	public:
		using QtWindow::QtWindow;

		LoginWindow(UI* engine);

		~LoginWindow(void) noexcept {}

	private:
		static constexpr int32_t HEIGHT = 230;
		static constexpr int32_t WIDTH = 250;

		QLineEdit* inputUsername{nullptr};
};