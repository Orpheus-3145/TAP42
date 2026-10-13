#pragma once

#include <vector>

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QSplitter>
#include <QFont>
#include <QSocketNotifier>

#include "Window.hpp"
#include "Message.hpp"


static std::vector<std::string> CMD_HINTS
{
	"LOOK",
	"MOVE",
	"WHO",
	"TAKE",
	"DROP",
	"INVENTORY",
	"TALK",
	"ATTACK",
	"STATUS",
	"QUEST",
	"QUESTS",
	"QUIT",
	"USE"
};

static std::vector<std::string> CHAT_CMD_HINTS
{
	"GROUP CREATE",
	"GROUP INVITE",
	"GROUP JOIN",
	"GROUP LEAVE",
	"CHAT GROUP",
	"CHAT ROOM",
	"CHAT GLOBAL",
	"QUIT"
};


// Stile "terminale" condiviso da tutti i pannelli: sfondo scuro, testo bianco,
// font monospace, bordo sottile bianco — per assomigliare all'immagine di riferimento.
static const QString kPanelStyle = R"(
    background-color: #1a1a1a;
    color: #e6e6e6;
    border: 1px solid #ffffff;
    font-family: "Courier New", monospace;
    font-size: 13px;
)";

class GameWindow : public Window
{
	Q_OBJECT

	public:
		using Window::Window;

		GameWindow(QWidget *parent, UI* engine, int32_t height, int32_t width);

		~GameWindow(void) noexcept {}

		void appendResponse(Message const& response, CommandType cmdType);
		void appendEvent(Message const& event);
		void appendError(Message const& error, CommandType cmdType);

	signals:
		void commandEntered(const QString &command);

	private slots:
		void onPromptSubmitted();

	private:
		void draw(int32_t height, int32_t width) override;

		QPlainTextEdit *inputLog;
		QLineEdit      *promptField;
		QPlainTextEdit *responsesLog;
		QPlainTextEdit *eventsLog;
};
