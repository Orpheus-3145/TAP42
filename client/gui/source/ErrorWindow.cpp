#include "ErrorWindow.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>
#include <functional>

#include <QVBoxLayout>
#include <QFrame>
#include <QString>
#include <QProxyStyle>
#include <QPushButton>


void ErrorWindow::draw(void)
{
	this->setObjectName("error");
	this->setFixedSize(ErrorWindow::WIDTH, ErrorWindow::HEIGHT);

	this->info = new QLabel();
	this->info->setObjectName("info");
	this->info->setAlignment(Qt::AlignCenter);

	this->btnContainer = new QHBoxLayout;

	QWidget*		frame = new QWidget(this);
	QVBoxLayout*	central = new QVBoxLayout(frame);

	central->setContentsMargins(16, 24, 16, 16);
	central->addWidget(this->info);
	central->addStretch(2);
	central->addStretch(1);
	central->addLayout(this->btnContainer);
	this->setCentralWidget(frame); 

	this->setStyleSheet(R"(
		QWidget#error { background-color: #12121c; }
		QLabel#info { color: #ff0000; }
		QPushButton { background: transparent; padding: 6px 6px; border-width: 1px; border-style: solid; color: #e74c3c; border-color: #e74c3c; }
		QPushButton:hover { background-color: rgba(46, 204, 113, 40); }
	)");

	LOG_DEBUG(LogContext::GUI, std::format("Showing error window, size h: {}, w: {}", this->height(), this->width()));
}

void ErrorWindow::adaptWinToError(ErrorData const& error) noexcept
{
	this->info->setText(QString(mapError(error).data()));
	while (QLayoutItem* item = this->btnContainer->takeAt(0))
	{
		if (QWidget* w = item->widget())
		{
			w->hide();
			w->deleteLater();
		}
		delete item;
	}

	switch (error.code)
	{
		case ErrorCode::UI_USERNAME_NOT_EXISTS:
			this->setAction1("RETRY", [this] { this->engine->switchWindow(GamePhase::LOGIN); });
			this->setAction2("CREATE NEW", [this] { this->engine->switchWindow(GamePhase::PLAYER_CREATE); });
			break;

		case ErrorCode::UI_USERNAME_IN_USE:
			this->setAction1("RETRY", [this] { this->engine->switchWindow(GamePhase::PLAYER_CREATE); });
			break;

		case ErrorCode::SERVER_DISCONNECTED:
			this->setAction1("CONNECT", [this] { this->engine->reconnect(); });
			this->setAction1("CLOSE", [this] { this->engine->stop(); });
			break;

		default:
			this->setAction1("CLOSE", [this] { this->engine->stop(); });
			break;
	}
}

void ErrorWindow::setAction1(std::string const& actionName, std::function<void()> action)
{
	QPushButton *btn1 = new QPushButton(QString(actionName.data()));
	btn1->setObjectName("btn1");
	btn1->setCursor(Qt::PointingHandCursor);
	QObject::connect(btn1, &QPushButton::clicked, this, action);

	this->btnContainer->addWidget(btn1);
}

void ErrorWindow::setAction2(std::string const& actionName, std::function<void()> action)
{
	QPushButton *btn2 = new QPushButton(QString(actionName.data()));
	btn2->setObjectName("btn2");
	btn2->setCursor(Qt::PointingHandCursor);
	QObject::connect(btn2, &QPushButton::clicked, this, action);

	this->btnContainer->addStretch();
	this->btnContainer->addWidget(btn2);
}
