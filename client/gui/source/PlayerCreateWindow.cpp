#include "PlayerCreateWindow.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


void PlayerCreateWindow::draw(void)
{
	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing player create window, size h: {}, w: {}", this->height(), this->width()));
}
