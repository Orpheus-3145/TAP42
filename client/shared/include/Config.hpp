#pragma once

#include <string>
#include <cstdint>

#include "Logger.hpp"


struct Config
{
	static constexpr size_t BUFF_SIZE = 1024UL;
	static constexpr const size_t CMD_BUFFER_SIZE = 128UL;
	static constexpr const char MSG_TERM = '\n';
	static constexpr const char MSG_SP = ' ';
	
	static constexpr char const* LOG_DIR = "client/logs";
	static constexpr LogLevel DEFAULT_LOG_LEVEL = LogLevel::DEBUG;

	static constexpr int32_t WIDTH_WIN = 100;
	static constexpr int32_t HEIGHT_WIN = 40;
};