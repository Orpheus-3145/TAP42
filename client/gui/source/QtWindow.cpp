#include "QtWindow.hpp"

#include <cassert>
#include <QShortcut>
#include <QKeySequence>
#include <QScreen>
#include <QShowEvent>


QtWindow::QtWindow(UI* engine) : 
	QMainWindow(nullptr),
	engine{engine}
{
	assert(this->engine != nullptr and "null pointer for engine instance received");

	this->setWindowFlag(Qt::WindowCloseButtonHint, false);
	QShortcut *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
	QObject::connect(
		escShortcut,
		&QShortcut::activated,
		this,
		[this] { this->engine->stop(); });
}

QtWindow::~QtWindow(void) noexcept
{
}

void QtWindow::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);

	if (event->spontaneous())
		return;

	// center window only when show is triggerd internally
	QRect area = this->screen()->availableGeometry();
	this->move(area.center() - this->rect().center());
}
