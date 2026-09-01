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

class ClientException : public AppException {
	public:
		using AppException::AppException;
};
