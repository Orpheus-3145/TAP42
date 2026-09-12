#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QSplitter>
#include <QFont>
#include <QSocketNotifier>


// Stile "terminale" condiviso da tutti i pannelli: sfondo scuro, testo bianco,
// font monospace, bordo sottile bianco — per assomigliare all'immagine di riferimento.
static const QString kPanelStyle = R"(
    background-color: #1a1a1a;
    color: #e6e6e6;
    border: 1px solid #ffffff;
    font-family: "Courier New", monospace;
    font-size: 13px;
)";

class GameWindow : public QMainWindow
{
    Q_OBJECT

	public:
		GameWindow(QWidget *parent, int32_t height, int32_t width);

		~GameWindow(void) noexcept {}

		void appendResponse(const QString &text)  { this->responsesLog->appendPlainText(text); }
		void appendEvent(const QString &text)     { this->eventsLog->appendPlainText(text); }

	signals:
		void commandEntered(const QString &command);

	private slots:
		void onPromptSubmitted();

	private:
		void draw(int32_t height, int32_t width);

		QPlainTextEdit *inputLog;
		QLineEdit      *promptField;
		QPlainTextEdit *responsesLog;
		QPlainTextEdit *eventsLog;
};
