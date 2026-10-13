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

		ErrorWindow(QWidget *parent, int32_t height, int32_t width, UI* engine) :
			Window(parent, height, width, engine) {}

		~ErrorWindow(void) noexcept {}

	// signals:
	// 	void commandEntered(const QString &command);

	// private slots:
	// 	void onPromptSubmitted();

	private:
		void draw(int32_t height, int32_t width) override;

		// QPlainTextEdit *inputLog;
		// QLineEdit      *promptField;
		// QPlainTextEdit *responsesLog;
		// QPlainTextEdit *eventsLog;
};

