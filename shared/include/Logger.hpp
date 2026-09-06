#pragma once

#include <string>
#include <mutex>
#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>


enum class LogLevel
{
	DEBUG	= 0,
	INFO	= 1,
	WARN	= 2,
	ERROR	= 3
};


enum class LogContext : uint32_t
{
	NONE			= 0,
	HTTP_CLIENT		= 1 << 0,
	GAME_CLIENT		= 1 << 1,
	INTERFACE		= 1 << 2,
	INPUT_OUTPUT	= 1 << 3,
};

constexpr bool operator==(LogContext a, LogContext b) noexcept
{
	return static_cast<uint32_t>(a) == static_cast<uint32_t>(b);
}

constexpr bool operator!=(LogContext a, LogContext b) noexcept
{
	return static_cast<uint32_t>(a) != static_cast<uint32_t>(b);
}

constexpr LogContext operator|(LogContext a, LogContext b) noexcept
{
	return static_cast<LogContext>(
		static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
	);
}

constexpr LogContext operator&(LogContext a, LogContext b) noexcept
{
	return static_cast<LogContext>(
		static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
	);
}

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
		void setFilter(LogContext filters) noexcept { this->filter = this->filter & filters; }

		void log(LogContext context, LogLevel level, const std::string& message) noexcept;
		void debug(LogContext context, const std::string& message) noexcept;
		void info(LogContext context, const std::string& message) noexcept;
		void warn(LogContext context, const std::string& message) noexcept;
		void error(LogContext context, const std::string& message) noexcept;

		static constexpr LogContext ALL_ENTRIES = LogContext::HTTP_CLIENT | LogContext::GAME_CLIENT | LogContext::INTERFACE | LogContext::INPUT_OUTPUT;

		static std::string to_string(LogLevel const& context) noexcept;
		static std::string to_string(LogContext const& context) noexcept;

	private:
		Logger(void) : minLevel(LogLevel::DEBUG), consoleOutput(true), fileEnabled(true) {}
		~Logger(void);

		static std::string currentTimestamp(void) noexcept;

		std::mutex		mtx;
		std::ofstream	fileStream;

		LogLevel	minLevel;
		LogContext	filter{Logger::ALL_ENTRIES};

		bool			consoleOutput;
		bool			fileEnabled;
};

#define LOG_DEBUG(context, msg) Logger::getInstance().debug(context, msg)
#define LOG_INFO(context, msg)  Logger::getInstance().info(context, msg)
#define LOG_WARN(context, msg)  Logger::getInstance().warn(context, msg)
#define LOG_ERROR(context, msg) Logger::getInstance().error(context, msg)
