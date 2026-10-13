#include "PlayerCreateWindow.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


void PlayerCreateWindow::draw(int32_t height, int32_t width)
{
	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing player create window, size h: {}, w: {}", height, width));
}
