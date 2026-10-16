#pragma once

#include <vector>

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QSplitter>
#include <QFont>
#include <QSocketNotifier>

#include "Window.hpp"
#include "Panel.hpp"
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

class GameWindow : public Window
{
	Q_OBJECT

	public:
		using Window::Window;

		~GameWindow(void) noexcept {}

		void setPlayerInfo(QString const& info);
		void appendResponse(Message const& response, CommandType cmdType);
		void appendEvent(Message const& event);
		void appendError(Message const& error, CommandType cmdType);

	signals:
		void commandEntered(std::string input);

	private:
		void draw(int32_t height, int32_t width) override;

		// panel that sent the oldest command still waiting for its response
		Panel* takePending(void);
 
		Panel* playerPanel{nullptr};	// top left, static text
		Panel* worldPanel{nullptr};		// bottom left, output only
		Panel* userPanel{nullptr};		// top right, with prompt
		Panel* chatPanel{nullptr};		// bottom right, with prompt
 
		std::queue<Panel*> pending;
};
