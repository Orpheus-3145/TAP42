#include "LoginWindow.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


void LoginWindow::draw(void)
{
	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing login window, size h: {}, w: {}", this->height(), this->width()));
}
