#include "GameWindow.hpp"
#include "UI.hpp"
#include "GUI.hpp"

#include <chrono>
#include <QStringList>


void GameWindow::draw(void)
{
	this->setWindowTitle("Game");

	QWidget* central = new QWidget(this);
	central->setObjectName("central");
	central->setStyleSheet("QWidget#central { background: #0f0f18; border: 1px solid #1f5fa8; }");
	this->setCentralWidget(central);

	this->playerPanel = new Panel("", BLUE_COLOR, false, false, false, central);
	this->playerPanel->setMaximumHeight(158);
	this->playerPanel->appendInput("Player: " + this->engine->getUsername());
	this->playerPanel->appendInput("Data: <CLASS | RACE | ...>");
	this->playerPanel->appendInput("Currently in: <LOCATION>");
	this->playerPanel->appendInput("<IN GROUP | NOT IN GROUP>");

	this->worldPanel = new Panel("World events", YELLOW_COLOR, false, true, false, central);
	this->cmdPanel = new Panel("User Events", CYAN_COLOR, true, true, true, central, CMD_HINTS);
	this->chatPanel = new Panel("Chat", GREEN_COLOR, true, true, true, central, CHAT_CMD_HINTS);

	QVBoxLayout* left = new QVBoxLayout();
	left->setSpacing(10);
	left->addWidget(this->playerPanel, 0);
	left->addWidget(this->worldPanel, 1);

	QVBoxLayout* right = new QVBoxLayout();
	right->setSpacing(10);
	right->addWidget(this->cmdPanel, 1);
	right->addWidget(this->chatPanel, 1);

	QHBoxLayout* root = new QHBoxLayout(central);
	root->setContentsMargins(20, 20, 20, 20);
	root->setSpacing(10);
	root->addLayout(left, 1);
	root->addLayout(right, 1);

	QObject::connect(
		this->cmdPanel,
		&Panel::submitted,
		this,
		[this] (QString const& text) { emit commandEntered(text.toStdString()); }
	);
	QObject::connect(
		this->chatPanel,
		&Panel::submitted,
		this,
		[this] (QString const& text) { emit commandEntered(text.toStdString()); }
	);

	this->cmdPanel->focusPrompt();
	QMainWindow::show();		// NB this should not be here

	LOG_DEBUG(LogContext::GUI, std::format("Showing game window, size h: {}, w: {}", this->height(), this->width()));
}

void GameWindow::appendResponse(Message const& response, CommandType cmdType)
{
	if ((cmdType == CommandType::GROUP_CHAT) or (cmdType == CommandType::ROOM_CHAT) or (cmdType == CommandType::GLOBAL_CHAT))
		return;

	std::string commandStr = toString(cmdType), responseStr = response.toStringResponse(cmdType), color;

	if (std::find(CMD_HINTS.begin(), CMD_HINTS.end(), commandStr) != CMD_HINTS.end())
		this->cmdPanel->appendInput(responseStr);
	else if (std::find(CHAT_CMD_HINTS.begin(), CHAT_CMD_HINTS.end(), commandStr) != CHAT_CMD_HINTS.end())
		this->chatPanel->appendInput(responseStr);
	else
		assert(false and "response doesn't belong to any panel");
}

void GameWindow::appendEvent(Message const& event)
{
	std::string eventStr = event.toStringEvent();
	this->worldPanel->appendInput(eventStr);
}

void GameWindow::appendError(Message const& error, CommandType cmdType)
{
	std::string commandStr = toString(cmdType), errorStr = error.toStringError();

	if (std::find(CMD_HINTS.begin(), CMD_HINTS.end(), commandStr) != CMD_HINTS.end())
		this->cmdPanel->appendInput(errorStr);
	else if (std::find(CHAT_CMD_HINTS.begin(), CHAT_CMD_HINTS.end(), commandStr) != CHAT_CMD_HINTS.end())
		this->chatPanel->appendInput(errorStr);
	else
		assert(false and "error doesn't belong to any panel");
}
