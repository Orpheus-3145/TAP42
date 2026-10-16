#include "GameWindow.hpp"


void GameWindow::draw(int32_t height, int32_t width)
{
	this->setWindowTitle("TAP 42");
	this->resize(width, height);
 
	QWidget* central = new QWidget(this);
	central->setObjectName("central");
	central->setStyleSheet("QWidget#central { background: #0f0f18; border: 1px solid #1f5fa8; }");
	this->setCentralWidget(central);
 
	// colors taken from the ncurses version
	this->playerPanel = new Panel("",              "#1f5fa8", false, "", central);
	this->worldPanel  = new Panel("World events",  "#b87333", false, "", central);
	this->userPanel   = new Panel("User Events",   "#1fa3b8", true,  "",   central);
	this->chatPanel   = new Panel("Chat",          "#1fa870", true,  "->", central);
 
	this->playerPanel->setMaximumHeight(130);
	this->setPlayerInfo(
		"Player:\n"
		"Data: <CLASS | RACE | ...>\n"
		"Currently in: <LOCATION>\n"
		"<IN GROUP | NOT IN GROUP>"
	);
 
	// left column: player info (fixed) + world events (takes the rest)
	QVBoxLayout* left = new QVBoxLayout();
	left->setSpacing(10);
	left->addWidget(this->playerPanel, 0);
	left->addWidget(this->worldPanel, 1);
 
	// right column: two equal panels, each with its own prompt
	QVBoxLayout* right = new QVBoxLayout();
	right->setSpacing(10);
	right->addWidget(this->userPanel, 1);
	right->addWidget(this->chatPanel, 1);
 
	QHBoxLayout* root = new QHBoxLayout(central);
	root->setContentsMargins(20, 20, 20, 20);
	root->setSpacing(10);
	root->addLayout(left, 1);
	root->addLayout(right, 1);
 
	// when a prompt is submitted: echo it in its own panel, remember who asked
	// (responses come back in order), then forward to GUI via Window::commandEntered
	for (Panel* p : {this->userPanel, this->chatPanel})
	{
		QObject::connect(
			p,
			&Panel::submitted,
			this,
			[this, p] (QString const& text) {
				p->appendInput(text);
				this->pending.push(p);
				emit commandEntered(text.toStdString());
			}
		);
	}
 
	this->userPanel->focusPrompt();
	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing game window, size h: {}, w: {}", height, width));
}

Panel* GameWindow::takePending(void)
{
	if (this->pending.empty())
		return this->userPanel;		// e.g. command sent from another window
 
	Panel* p = this->pending.front();
	this->pending.pop();
	return p;
}
 
void GameWindow::setPlayerInfo(QString const& info)
{
	this->playerPanel->setText(info);
}
 
void GameWindow::appendResponse(Message const& response, CommandType cmdType)
{
	std::string content = response.toStringResponse(cmdType);
	this->takePending()->appendLine(QString::fromStdString(content));
}
 
void GameWindow::appendEvent(Message const& event)
{
	std::string content = event.toStringEvent();
	this->worldPanel->appendLine(QString::fromStdString(content));
}
 
void GameWindow::appendError(Message const& error, CommandType cmdType)
{
	(void) cmdType;
	std::string content = error.toStringError();
	this->takePending()->appendLine(QString::fromStdString(content));
}