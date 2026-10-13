#include "Window.hpp"
#include "Logger.hpp"
#include "UI.hpp"

#include <cassert>
#include <format>


Window::Window(QWidget *parent, UI* engine) : 
	QMainWindow(parent),
	engine{engine}
{
	assert(this->engine != nullptr and "null pointer for engine instance received");
}

Window::~Window(void) noexcept
{
}
