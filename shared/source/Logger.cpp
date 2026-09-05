#include "Logger.hpp"
#include "Exceptions.hpp"

#include <iostream>
#include <iomanip>
#include <ctime>
#include <chrono>


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
	std::cout << path << std::endl;
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

void Logger::log(LogLevel level, const std::string& message) noexcept
{
	std::lock_guard<std::mutex> lock(this->mtx);

	if (level < this->minLevel)
		return;

	std::ostringstream line;
	line << "[" << currentTimestamp() << "] "
		 << "[" << levelToString(level) << "] "
		 << message;

	// if (this->consoleOutput)
	// {
	// 	if (level == LogLevel::ERROR)
	// 		std::cerr << line.str() << std::endl;
	// 	else
	// 		std::cout << line.str() << std::endl;
	// }

	if (this->fileEnabled)
	{
		this->fileStream << line.str() << std::endl;
		this->fileStream.flush();
	}
}

void Logger::debug(const std::string& message) noexcept { this->log(LogLevel::DEBUG, message); }
void Logger::info(const std::string& message) noexcept  { this->log(LogLevel::INFO, message); }
void Logger::warn(const std::string& message) noexcept  { this->log(LogLevel::WARN, message); }
void Logger::error(const std::string& message) noexcept { this->log(LogLevel::ERROR, message); }

std::string Logger::levelToString(LogLevel level) noexcept
{
	switch (level)
	{
		case LogLevel::DEBUG: return "DEBUG";
		case LogLevel::INFO:  return "INFO ";
		case LogLevel::WARN:  return "WARN ";
		case LogLevel::ERROR: return "ERROR";
	}
	return "?????";
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