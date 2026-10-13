#pragma once

#include <cstdint>

#include "Window.hpp"


class LoginWindow : public Window
{
    Q_OBJECT

	public:
		using Window::Window;

		LoginWindow(QWidget *parent, UI* engine, int32_t height, int32_t width);

		~LoginWindow(void) noexcept {}

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