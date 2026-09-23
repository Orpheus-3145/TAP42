#include "Exceptions.hpp"

#include <format>


std::string mapError(ErrorCode code, std::string const& errorData) noexcept
{
	switch (code)
	{
		case ErrorCode::FILE_NOT_FOUND:				return std::format("Path '{}' not found", errorData);
		case ErrorCode::BAD_FORMAT_ARGS:			return std::format("Invalid arg format: {}", errorData);
		case ErrorCode::CLIENT_DISCONNECTED:		return std::format("Client HTTP disconnected", errorData);
		case ErrorCode::SERVER_DISCONNECTED:		return std::format("Server disconnected", errorData);
		case ErrorCode::SERVER_CONN_FAILED:			return std::format("Connection to server failed: {}", errorData);
		case ErrorCode::SERVER_ERROR:				return std::format("Server error: {}", errorData);
		case ErrorCode::IO_READ_FAILED:				return std::format("Read error: {}", errorData);
		case ErrorCode::IO_WRITE_FAILED:			return std::format("Write error: {}", errorData);
		case ErrorCode::IO_READ_NONB_FAILED:		return std::format("Recv error: {}", errorData);
		case ErrorCode::IO_WRITE_NONB_FAILED:		return std::format("Send error: {}", errorData);
		case ErrorCode::IO_POLL_FAILED:				return std::format("Poll error: {}", errorData);
		case ErrorCode::IO_PIPE_FAILED:				return std::format("Piping data failed: {}", errorData);
		case ErrorCode::IO_PIPE_CREATE_FAILED:		return std::format("Failed to create pipe: {}", errorData);
		case ErrorCode::IO_SOCK_CREATE_FAILED:		return std::format("Failed to create socket: {}", errorData);
		case ErrorCode::IO_SIG_SOCK_CREATE_FAILED:	return std::format("Failed to create signal redirected fd: {}", errorData);
		case ErrorCode::UI_INVALID_SIZE:			return std::format("UI widget couldn't be drawn (newinw() failed): {}", errorData);
		case ErrorCode::UI_HANDSHAKE_NOT_DONE:		return std::format("No handshake received before performing action", errorData);
		case ErrorCode::UI_USERNAME_NOT_EXISTS:		return std::format("Username doesn't exist", errorData);
		case ErrorCode::UI_FAILED_GET_TERM_SIZE:	return std::format("Couldn't get terminal size", errorData);
		default:									return std::format("No error found for code: {}", errorData);
	}
}

AppException::AppException(ErrorCode code, std::string const& errorData) noexcept :
	std::runtime_error(mapError(code, errorData)),
	code{code}
{
}
