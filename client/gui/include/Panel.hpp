#pragma once

#include <string>
#include <deque>
#include <optional>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QStringList>
#include <QString>


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

		void appendInput(std::string const& text, std::optional<std::string> color = std::nullopt, bool addTimestamps = false);
		void refresh(void);
		void clear(void) noexcept { this->log->setPlainText(""); }
		void focusPrompt(void) { if (this->prompt) this->prompt->setFocus(); }

	signals:
		void submitted(const QString& text);

	private:
		void submitCommand(void);

		QString			color;
		bool			showTimestamps, checkInput;
		QStringList		hints{};

		std::deque<std::string>	state;
		
		QPlainTextEdit* log{nullptr};
		QLineEdit*      prompt{nullptr};
};
