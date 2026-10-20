#include "GameWindow.hpp"
#include "UI.hpp"
#include "GUI.hpp"

#include <format>
#include <cassert>
#include <QWidget>
#include <QHBoxLayout>


GameWindow::GameWindow(UI* engine) : QtWindow(engine)
{
	this->setObjectName("game");
	this->resize(GameWindow::WIDTH, GameWindow::HEIGHT);

	this->playerPanel = std::make_unique<Panel>("", QT_BLUE_COLOR, false, false, false);
	this->playerPanel->setMaximumHeight(135);
	this->worldPanel = std::make_unique<Panel>("World events", QT_YELLOW_COLOR, false, true, false);
	this->cmdPanel = std::make_unique<Panel>("User Events", QT_CYAN_COLOR, true, true, true, nullptr, CMD_HINTS);
	this->chatPanel = std::make_unique<Panel>("Chat", QT_GREEN_COLOR, true, true, true, nullptr, CHAT_CMD_HINTS);

	QWidget* central = new QWidget(this);
	central->setObjectName("central");
	central->setStyleSheet(std::format("QWidget#central {{ background: #0f0f18; border: 1px solid {}; }}", QT_BLUE_COLOR).data());
	this->setCentralWidget(central);

	QVBoxLayout* left = new QVBoxLayout();
	left->setSpacing(10);
	left->addWidget(this->playerPanel.get(), 0);
	left->addWidget(this->worldPanel.get(), 1);

	QVBoxLayout* right = new QVBoxLayout();
	right->setSpacing(10);
	right->addWidget(this->cmdPanel.get(), 1);
	right->addWidget(this->chatPanel.get(), 1);

	QHBoxLayout* root = new QHBoxLayout(central);
	root->setContentsMargins(20, 20, 20, 20);
	root->setSpacing(10);
	root->addLayout(left, 1);
	root->addLayout(right, 1);

	QObject::connect(
		this->cmdPanel.get(),
		&Panel::submitted,
		this,
		[this] (QString const& text) { emit commandEntered(text.toStdString()); }
	);
	QObject::connect(
		this->chatPanel.get(),
		&Panel::submitted,
		this,
		[this] (QString const& text) { emit commandEntered(text.toStdString()); }
	);

	LOG_DEBUG(LogContext::GUI, std::format("Created game Qt, size w: {}, h: {}", GameWindow::WIDTH, GameWindow::HEIGHT));
}

void GameWindow::appendResponse(Message const& response, CommandType cmdType)
{
	if ((cmdType == CommandType::GROUP_CHAT) or (cmdType == CommandType::ROOM_CHAT) or (cmdType == CommandType::GLOBAL_CHAT))
		return;

	std::string commandStr = toString(cmdType), responseStr = response.toStringResponse(cmdType);

	Panel* toUpdate = nullptr;
	if (std::find(CMD_HINTS.begin(), CMD_HINTS.end(), commandStr) != CMD_HINTS.end())
		toUpdate = this->cmdPanel.get();
	else if (std::find(CHAT_CMD_HINTS.begin(), CHAT_CMD_HINTS.end(), commandStr) != CHAT_CMD_HINTS.end())
		toUpdate = this->chatPanel.get();
	assert(toUpdate and "response doesn't belong to any panel");

	toUpdate->appendInput(responseStr);
}

void GameWindow::appendEvent(Message const& event)
{
	std::string eventStr = event.toStringEvent();
	this->worldPanel->appendInput(eventStr, std::nullopt, true);
}

void GameWindow::appendError(Message const& error, CommandType cmdType)
{
	std::string commandStr = toString(cmdType), errorStr = error.toStringError();

	Panel* toUpdate = nullptr;
	if (std::find(CMD_HINTS.begin(), CMD_HINTS.end(), commandStr) != CMD_HINTS.end())
		toUpdate = this->cmdPanel.get();
	else if (std::find(CHAT_CMD_HINTS.begin(), CHAT_CMD_HINTS.end(), commandStr) != CHAT_CMD_HINTS.end())
		toUpdate = this->chatPanel.get();
	assert(toUpdate and "error doesn't belong to any panel");

	toUpdate->appendInput(errorStr);
}

void GameWindow::updatePlayerPanel(PlayerInfo const& data)
{
	this->playerPanel->clear();
	this->playerPanel->appendInput("Player: " + data.username);
	this->playerPanel->appendInput("HP: " + std::to_string(data.hp));
	this->playerPanel->appendInput("Currently in: " + data.location);
	this->playerPanel->appendInput((data.group.empty() ? "Not in group" : ("Member of: " + data.group)));
}

void GameWindow::showEvent(QShowEvent* event)
{
	QtWindow::showEvent(event);

	if (event->spontaneous())
		return;

	this->playerPanel->refresh();
	this->cmdPanel->refresh();
	this->worldPanel->refresh();
	this->chatPanel->refresh();
	this->cmdPanel->focusPrompt();
}
