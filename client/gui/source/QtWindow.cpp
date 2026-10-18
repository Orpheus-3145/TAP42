#include "QtWindow.hpp"
#include "Logger.hpp"
#include "UI.hpp"

#include <cassert>
#include <format>


QtWindow::QtWindow(int32_t height, int32_t width, UI* engine, QWidget *parent) : 
	QMainWindow(parent),
	engine{engine}
{
	assert(this->engine != nullptr and "null pointer for engine instance received");

	this->resize(width, height);
}

QtWindow::~QtWindow(void) noexcept
{
}
