#include "Window.hpp"
#include "Logger.hpp"
#include "UI.hpp"

#include <cassert>
#include <format>


Window::Window(int32_t height, int32_t width, UI* engine, QWidget *parent) : 
	QMainWindow(parent),
	engine{engine}
{
	assert(this->engine != nullptr and "null pointer for engine instance received");

	this->resize(width, height);
}

Window::~Window(void) noexcept
{
}
