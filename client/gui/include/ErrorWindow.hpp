#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <QLabel>
#include <QHBoxLayout>

#include "QtWindow.hpp"
#include "UI.hpp"
#include "Exceptions.hpp"


class ErrorWindow : public QtWindow
{
    Q_OBJECT

	public:
		using QtWindow::QtWindow;

		~ErrorWindow(void) noexcept {}

		void draw(void) override;

		void adaptWinToError(ErrorData const& error) noexcept;
		void setAction1(std::string const& actionName, std::function<void()> action);
		void setAction2(std::string const& actionName, std::function<void()> action);

	private:
		static constexpr int32_t HEIGHT = 230;
		static constexpr int32_t WIDTH = 250;

		QLabel		*info{nullptr};
		QHBoxLayout	*btnContainer{nullptr};
};
