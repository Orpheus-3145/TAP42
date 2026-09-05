#pragma once

#include <string>
#include <cstdint>

#include "Logger.hpp"


struct Config
{
	static constexpr size_t R_BUFF_SIZE = 1024UL;
	static constexpr const char MSG_TERM = '\n';
	static constexpr const char SP = ' ';
	static constexpr char const* QUIT = "quit";
	
	static constexpr char const* LOG_DIR = "logs/";
	static constexpr LogLevel DEFAULT_LOG_LEVEL = LogLevel::DEBUG;
};