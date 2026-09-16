#pragma once

#include <string>
#include <cstdint>

#include "Logger.hpp"


struct Config
{
	static constexpr const size_t BUFF_SIZE = 1024UL;
	static constexpr const size_t CMD_BUFFER_SIZE = 128UL;

	static constexpr const char	COMMAND_TERM = '\n';
	static constexpr const char COMMAND_SP = ' ';

	static constexpr char const* PROMPT = "-> ";

	static constexpr char const*	LOG_DIR = "logs";
	static constexpr LogLevel		DEFAULT_LOG_LEVEL = LogLevel::DEBUG;

	static constexpr int32_t WIDTH_GUI = 1100;
	static constexpr int32_t HEIGHT_GUI = 600;
};
