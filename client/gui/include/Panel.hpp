#pragma once

#include <string>
#include <optional>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStringList>
#include <QString>
#include <QVBoxLayout>


class Panel : public QFrame
{
	Q_OBJECT

	public:
		Panel(
			QString const& title,
			QString const& color,
			bool withInput = false,
			bool showTimestamps = false,
			bool checkInput = false,
			QWidget* parent = nullptr,
			std::optional<std::vector<std::string>> hints = std::nullopt
		);

		void appendInput(std::string const& text, std::optional<std::string> color = std::nullopt);
		void focusPrompt(void) { if (this->prompt) this->prompt->setFocus(); }

	signals:
		void submitted(const QString& text);

	private:
		void printLine(std::string const& text);
		void submitCommand(void);

		QString			color;
		bool			showTimestamps, checkInput;
		QStringList		hints{};
		
		QPlainTextEdit* log{nullptr};
		QLineEdit*      prompt{nullptr};
};
