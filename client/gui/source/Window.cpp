#include "Window.hpp"
#include "Logger.hpp"
#include "UI.hpp"

#include <cassert>
#include <format>


Window::Window(QWidget *parent, int32_t height, int32_t width, UI* engine) : 
	QMainWindow(parent),
	engine{engine}
{
	assert(this->engine != nullptr and "Null pointer for engine instance received");

	// signal when to app when user presses enter on command input 
	QObject::connect(
		this->promptField,
		&QLineEdit::returnPressed,
		this,
		&Window::onPromptSubmitted
	);
	this->draw(height, width);
}

Window::~Window(void) noexcept
{
}
