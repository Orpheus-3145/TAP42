#include "LoginWindow.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFrame>
#include <QProxyStyle>


void LoginWindow::draw(void)
{
	setObjectName("login");
	setFixedSize(LoginWindow::WIDTH, LoginWindow::HEIGHT);

	QLabel* title = new QLabel("Enter username:");
	title->setObjectName("title");
	title->setAlignment(Qt::AlignCenter);

	QLabel* prompt = new QLabel("->");
	prompt->setObjectName("prompt");

	this->inputUsername = new QLineEdit;
	this->inputUsername->setFrame(false);
	this->inputUsername->setFocus();

	QFrame* inputFrame = new QFrame;
	inputFrame->setObjectName("inputFrame");
	QHBoxLayout* inputLayout = new QHBoxLayout(inputFrame);
	inputLayout->setContentsMargins(8, 6, 8, 6);
	inputLayout->addWidget(prompt);
	inputLayout->addWidget(this->inputUsername);

	QPushButton* createBtn = new QPushButton("CREATE NEW");
	createBtn->setObjectName("createBtn");
	createBtn->setCursor(Qt::PointingHandCursor);

	QPushButton* closeBtn = new QPushButton("CLOSE");
	closeBtn->setObjectName("closeBtn");
	closeBtn->setCursor(Qt::PointingHandCursor);

	QHBoxLayout* buttons = new QHBoxLayout;
	buttons->addWidget(createBtn);
	buttons->addStretch();
	buttons->addWidget(closeBtn);

	QWidget* frame = new QWidget(this);
	QVBoxLayout* central = new QVBoxLayout(frame);
	central->setContentsMargins(16, 24, 16, 16);
	central->addWidget(title);
	central->addStretch(2);
	central->addWidget(inputFrame);
	central->addStretch(1);
	central->addLayout(buttons);
	this->setCentralWidget(frame); 

	// --- Stile ---
	this->setStyleSheet(R"(
		QWidget#login { background-color: #12121c; }
		QLabel#title { color: #ffffff; font-weight: bold; }
		QLabel#prompt { color: #3b82f6; }
		QFrame#inputFrame { border: 1px solid #1e4a8a; background-color: #14141f; }
		QLineEdit { background: transparent; border: none; color: #dddddd; selection-background-color: #3b82f6; }
		QPushButton { background: transparent; padding: 6px 6px; border-width: 1px; border-style: solid; }
		QPushButton#createBtn { color: #2ecc71; border-color: #3a8a5c; }
		QPushButton#createBtn:hover { background-color: rgba(46, 204, 113, 40); }
		QPushButton#closeBtn { color: #e74c3c; border-color: #e74c3c; }
		QPushButton#closeBtn:hover { background-color: rgba(231, 76, 60, 40); }
	)");

	QObject::connect(
		this->inputUsername,
		&QLineEdit::returnPressed,
		this,
		[this] {
			QString const text = this->inputUsername->text();
			if (text.isEmpty())
				return;

			this->inputUsername->clear();
			emit commandEntered(text.toStdString());
		}
	);
	QObject::connect(
		closeBtn,
		&QPushButton::clicked,
		this,
		[this] { this->engine->stop(); }
	);
	QObject::connect(
		createBtn,
		&QPushButton::clicked,
		this,
		[this] { this->engine->switchWindow(GamePhase::PLAYER_CREATE); }
	);

	LOG_DEBUG(LogContext::GUI, std::format("Showing login window, size h: {}, w: {}", LoginWindow::HEIGHT, LoginWindow::WIDTH));
}
