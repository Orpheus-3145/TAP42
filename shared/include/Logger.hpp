#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <sstream>


enum class LogLevel
{
	DEBUG = 0,
	INFO  = 1,
	WARN  = 2,
	ERROR = 3
};

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

		void log(LogLevel level, const std::string& message) noexcept;
		void debug(const std::string& message) noexcept;
		void info(const std::string& message) noexcept;
		void warn(const std::string& message) noexcept;
		void error(const std::string& message) noexcept;

	private:
		Logger(void) : minLevel(LogLevel::DEBUG), consoleOutput(false), fileEnabled(true) {}
		~Logger(void);

		static std::string levelToString(LogLevel level) noexcept;
		static std::string currentTimestamp(void) noexcept;

		std::mutex		mtx;
		std::ofstream	fileStream;
		LogLevel		minLevel;
		bool			consoleOutput;
		bool			fileEnabled;
};

#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_INFO(msg)  Logger::getInstance().info(msg)
#define LOG_WARN(msg)  Logger::getInstance().warn(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)
