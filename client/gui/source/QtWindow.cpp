#include "QtWindow.hpp"

#include <cassert>
#include <QShortcut>
#include <QKeySequence>


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
		[this]() { this->engine->stop(); });
}

QtWindow::~QtWindow(void) noexcept
{
}
