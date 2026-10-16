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

	this->draw(height, width);

	// signal when user presses enter on command input 
	// QObject::connect(
	// 	this->promptField,
	// 	&QLineEdit::returnPressed,
	// 	this,
	// 	&Window::onPromptSubmitted
	// );
}

// void Window::onPromptSubmitted(void)
// {
// 	QString text = this->promptField->text();
// 	if (text.isEmpty())
// 		return;

// 	emit commandEntered(text.toStdString());
// 	this->promptField->clear();
// }

Window::~Window(void) noexcept
{
}

void Window::draw(int32_t height, int32_t width)
{
	this->setWindowTitle("TAP 42");
	this->resize(width, height);

	// this->addWidget(this->promptField, /*stretch=*/0);

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing window, size h: {}, w: {}", height, width));
}
