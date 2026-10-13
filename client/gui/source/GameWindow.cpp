#include "GameWindow.hpp"


GameWindow::GameWindow(QWidget *parent, int32_t height, int32_t width, UI* engine) :
	Window(parent, height, width, engine)
{
	// signal when to app when user presses enter on command input 
	QObject::connect(
		this->promptField,
		&QLineEdit::returnPressed,
		this,
		&GameWindow::onPromptSubmitted
	);
}

void GameWindow::onPromptSubmitted()
{
	QString text = this->promptField->text();
	if (text.isEmpty())
		return;

	emit commandEntered(text);

	this->promptField->clear();
}

void GameWindow::draw(int32_t height, int32_t width)
{
	setWindowTitle("TAP 42");
	resize(width, height);

	QWidget *central = new QWidget(this);
	setCentralWidget(central);

	QHBoxLayout *rootLayout = new QHBoxLayout(central);
	rootLayout->setContentsMargins(10, 10, 10, 10);
	rootLayout->setSpacing(10);

	QWidget *leftPanel = new QWidget(central);
	QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	leftLayout->setSpacing(0);

	this->inputLog = new QPlainTextEdit(leftPanel);
	this->inputLog->setReadOnly(true);
	this->inputLog->setStyleSheet(kPanelStyle);
	this->inputLog->setPlainText("This is where the input is shown");

	this->promptField = new QLineEdit(leftPanel);
	this->promptField->setStyleSheet(kPanelStyle);
	this->promptField->setFrame(false);

	leftLayout->addWidget(this->inputLog, /*stretch=*/1);
	leftLayout->addWidget(this->promptField, /*stretch=*/0);

	QSplitter *rightSplitter = new QSplitter(Qt::Vertical, central);

	this->responsesLog = new QPlainTextEdit(rightSplitter);
	this->responsesLog->setReadOnly(true);
	this->responsesLog->setStyleSheet(kPanelStyle);
	this->responsesLog->setPlainText("This is where responses are shown");

	this->eventsLog = new QPlainTextEdit(rightSplitter);
	this->eventsLog->setReadOnly(true);
	this->eventsLog->setStyleSheet(kPanelStyle);
	this->eventsLog->setPlainText("This is where events are shown");

	rightSplitter->addWidget(this->responsesLog);
	rightSplitter->addWidget(this->eventsLog);
	rightSplitter->setSizes({600, 400});

	rootLayout->addWidget(leftPanel, /*stretch=*/1);
	rootLayout->addWidget(rightSplitter, /*stretch=*/1);
}

void GameWindow::appendResponse(Message const& response, CommandType cmdType)
{
	// std::string strCommand = toString(cmdType);
	// find which tab the response belongs to
	std::string content = response.toStringResponse(cmdType);
	this->responsesLog->appendPlainText(QString::fromStdString(content));
}

void GameWindow::appendEvent(Message const& event)
{
	std::string content = event.toStringEvent();
	this->eventsLog->appendPlainText(QString::fromStdString(content));
}

void GameWindow::appendError(Message const& error, CommandType cmdType)
{
	(void) cmdType;
	// std::string strCommand = toString(cmdType);
	// find which tab the error belongs to
	std::string content = error.toStringError();
	this->responsesLog->appendPlainText(QString::fromStdString(content));
}
