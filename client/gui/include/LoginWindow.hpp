#pragma once

#include <cstdint>

#include "Window.hpp"


class LoginWindow : public Window
{
    Q_OBJECT

	public:
		using Window::Window;

		LoginWindow(QWidget *parent, int32_t height, int32_t width, UI* engine);

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