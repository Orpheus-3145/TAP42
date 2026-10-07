#include "Window.hpp"
#include "Logger.hpp"
#include "UI.hpp"

#include <cassert>
#include <format>


Window::Window(QWidget *parent, int32_t height, int32_t width, UI* engine) : 
	QMainWindow(parent),
	engine{engine}
{
	(void) height;
	(void) width;
	assert(this->engine != nullptr and "Null pointer for engine instance received");
}

Window::~Window(void) noexcept
{
}
