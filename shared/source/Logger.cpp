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
		case LogContext::HTTP: os << "CLIENT_HTTP"; break;
		case LogContext::GAME: os << "GAME"; break;
		case LogContext::UI: os << "INTERFACE"; break;
		case LogContext::IO: os << "READ_WRITE_OP"; break;
		case LogContext::GENERAL: os << "GENERAL"; break;
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

	std::ostringstream line;
	line << "[" << currentTimestamp() << "] "
		 << "[" << level << "] "
		 << "[" << context << "] "
		 << message;

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