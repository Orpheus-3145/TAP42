#include "ErrorWindow.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


void ErrorWindow::draw(void)
{
	LOG_DEBUG(LogContext::GUI, std::format("Showing error window, size h: {}, w: {}", this->height(), this->width()));
}
