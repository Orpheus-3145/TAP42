#pragma once

#include <string>
#include <cstdint>

struct Config
{
	static constexpr size_t R_BUFF_SIZE = 1024UL;
	static constexpr const char MSG_TERM = '\n';
	static constexpr const char SP = ' ';
	static constexpr char const* QUIT = "quit";
};