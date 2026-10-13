#include "PlayerCreateWindow.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


PlayerCreateWindow::PlayerCreateWindow(QWidget *parent, UI* engine, int32_t height, int32_t width) :
	Window(parent, engine)
{
	this->draw(height, width);
}

void PlayerCreateWindow::draw(int32_t height, int32_t width)
{
	(void) height;
	(void) width;

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing player create window, size h: {}, w: {}", height, width));
}
