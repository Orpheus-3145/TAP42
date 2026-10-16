#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QString>
#include <QVBoxLayout>

// A bordered panel like the ncurses ones: optional title + horizontal rule,
// a read-only log and (optionally) a prompt at the bottom.
// Header-only: just make sure it's listed in your sources/HEADERS so AUTOMOC sees it.
class Panel : public QFrame
{
	Q_OBJECT

	public:
		Panel(
			QString const& title,
			QString const& color,
			bool withInput = false,
			QString const& promptPrefix = QString(),
			QWidget* parent = nullptr
		);

		// replaces the whole content (used for the static "Player" panel)
		void setText(QString const& text) { this->log->setPlainText(text); }
		// echo of what the user typed
		void appendInput(QString const& text) { this->log->appendPlainText("> " + text); }
		// responses / events
		void appendLine(QString const& text) { this->log->appendPlainText(text); }
		void focusPrompt(void) { if (this->prompt) this->prompt->setFocus(); }

	signals:
		void submitted(const QString& text);

	private:
		QPlainTextEdit* log{nullptr};
		QLineEdit*      prompt{nullptr};
};