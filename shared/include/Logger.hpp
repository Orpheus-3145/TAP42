#pragma once

#include <string>
#include <mutex>
#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>


enum class LogLevel
{
	DEBUG = 0,
	INFO = 1,
	WARN = 2,
	ERROR = 3
};

enum class LogContext
{
	HTTP = 0,
	GAME  = 1,
	UI  = 2,
	IO = 3,
	GENERAL  = 4,
};

std::ostream& operator<<(std::ostream& os, LogLevel level) noexcept;
std::ostream& operator<<(std::ostream& os, LogContext context) noexcept;


class Logger		// Singleton
{
	public:
		Logger(const Logger&) = delete;
		Logger& operator=(const Logger&) = delete;
		Logger(Logger&&) = delete;
		Logger& operator=(Logger&&) = delete;

		static Logger& getInstance(void) noexcept;

		void setLogFile(const std::string& path);
		void setMinLevel(LogLevel level) noexcept;
		void setConsoleOutput(bool enabled) noexcept;

		void log(LogContext context, LogLevel level, const std::string& message) noexcept;
		void debug(LogContext context, const std::string& message) noexcept;
		void info(LogContext context, const std::string& message) noexcept;
		void warn(LogContext context, const std::string& message) noexcept;
		void error(LogContext context, const std::string& message) noexcept;

	private:
		Logger(void) : minLevel(LogLevel::DEBUG), consoleOutput(true), fileEnabled(true) {}
		~Logger(void);

		// static std::string levelToString(LogLevel level) noexcept;
		static std::string currentTimestamp(void) noexcept;

		std::mutex		mtx;
		std::ofstream	fileStream;
		LogLevel		minLevel;
		bool			consoleOutput;
		bool			fileEnabled;
};

#define LOG_DEBUG(context, msg) Logger::getInstance().debug(context, msg)
#define LOG_INFO(context, msg)  Logger::getInstance().info(context, msg)
#define LOG_WARN(context, msg)  Logger::getInstance().warn(context, msg)
#define LOG_ERROR(context, msg) Logger::getInstance().error(context, msg)
