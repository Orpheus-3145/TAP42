#pragma once

#include <string>
#include <stdexcept>


class AppException : public std::runtime_error {
	public:
		explicit AppException(std::string const& error ) : std::runtime_error(error) {};
};

class ParsingException : public AppException {
	public:
		using AppException::AppException;
};

class HTTPException : public AppException {
	public:
		using AppException::AppException;
};

class CLIException : public AppException {
	public:
		using AppException::AppException;
};

class IOException : public AppException {
	public:
		using AppException::AppException;
};

class ReadException : public IOException {
	public:
		using IOException::IOException;
};

class WriteException : public IOException {
	public:
		using IOException::IOException;
};

class BufferOverflowException : public IOException {
	public:
		using IOException::IOException;
};

class ConnClosedException : public IOException {
	public:
		using IOException::IOException;
};
