#include "Logger.hpp"
#include "Exceptions.hpp"

#include <iostream>
#include <iomanip>
#include <ctime>
#include <chrono>


std::ostream& operator<<(std::ostream& os, LogLevel level) noexcept
{
	switch (level)
	{
		case LogLevel::DEBUG: os << "DEBUG"; break;
		case LogLevel::INFO: os << "INFO"; break;
		case LogLevel::WARN: os << "WARN"; break;
		case LogLevel::ERROR: os << "ERROR"; break;
	}
	return os;
}


std::ostream& operator<<(std::ostream& os, LogContext context) noexcept
{
	switch (context)
	{
		case LogContext::NONE: os << "NONE"; break;
		case LogContext::HTTP_CLIENT: os << "HTTP_CLIENT"; break;
		case LogContext::GAME_CLIENT: os << "GAME_CLIENT"; break;
		case LogContext::INTERFACE: os << "INTERFACE"; break;
		case LogContext::INPUT_OUTPUT: os << "INPUT_OUTPUT"; break;
	}
	return os;
}

Logger::~Logger(void)
{
	if (this->fileStream.is_open())
		this->fileStream.close();
}

Logger& Logger::getInstance(void) noexcept
{
	static Logger instance;
	return instance;
}

void Logger::setLogFile(const std::string& path)
{
	std::lock_guard<std::mutex> lock(this->mtx);

	this->fileStream.open(path, std::ios::out | std::ios::app);
	this->fileEnabled = this->fileStream.is_open();
	if (!this->fileEnabled)
		AppException("Couldn't open log file: " + path);
}

void Logger::setMinLevel(LogLevel level) noexcept
{
	std::lock_guard<std::mutex> lock(this->mtx);
	this->minLevel = level;
}

void Logger::setConsoleOutput(bool enabled) noexcept
{
	std::lock_guard<std::mutex> lock(this->mtx);
	this->consoleOutput = enabled;
}

void Logger::log(LogContext context, LogLevel level, const std::string& message) noexcept
{
	std::lock_guard<std::mutex> lock(this->mtx);

	if (level < this->minLevel)
		return;
	else if ((this->filter & context) == LogContext::NONE)
		return;

	constexpr int32_t kTimestampWidth = 26;
	constexpr int32_t kLevelWidth  = 8;
	constexpr int32_t kContextWidth = 14;
	std::ostringstream line;

	auto padField = [](const std::string& content, int width) {
		std::ostringstream tmp;
		tmp << std::left << std::setw(width) << content;
		return tmp.str();
	};
	
	line << padField("[" + currentTimestamp() + "]", kTimestampWidth) << ' '
		<< padField("[" + Logger::to_string(level) + "]", kLevelWidth) << ' '
		<< padField("[" + Logger::to_string(context) + "]", kContextWidth)
		<< " - " << message;

	if (this->consoleOutput)
	{
		if (level == LogLevel::ERROR)
			std::cerr << line.str() << std::endl;
		else
			std::cout << line.str() << std::endl;
	}

	if (this->fileEnabled)
	{
		this->fileStream << line.str() << std::endl;
		this->fileStream.flush();
	}
}

void Logger::debug(LogContext context, const std::string& message) noexcept { this->log(context, LogLevel::DEBUG, message); }
void Logger::info(LogContext context, const std::string& message) noexcept  { this->log(context, LogLevel::INFO, message); }
void Logger::warn(LogContext context, const std::string& message) noexcept  { this->log(context, LogLevel::WARN, message); }
void Logger::error(LogContext context, const std::string& message) noexcept { this->log(context, LogLevel::ERROR, message); }

std::string Logger::to_string(LogLevel const& level) noexcept
{
	std::ostringstream line;
	line << level;
	return line.str();
}

std::string Logger::to_string(LogContext const& context) noexcept
{
	std::ostringstream line;
	line << context;
	return line.str();
}

std::string Logger::currentTimestamp(void) noexcept
{
	auto now = std::chrono::system_clock::now();
	auto nowTimeT = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;

	std::tm tmBuf;
	localtime_r(&nowTimeT, &tmBuf); // thread-safe

	std::ostringstream oss;
	oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S");
	oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
	return oss.str();
}