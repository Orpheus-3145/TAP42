#pragma once

#include <cstdint>

#include "Window.hpp"


class PlayerCreateWindow : public Window
{
    Q_OBJECT

	public:
		using Window::Window;

		PlayerCreateWindow(QWidget *parent, UI* engine, int32_t height, int32_t width);

		~PlayerCreateWindow(void) noexcept {}

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
