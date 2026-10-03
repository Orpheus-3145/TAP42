#include "Exceptions.hpp"

#include <format>


std::string mapError(ErrorData const& error) noexcept
{
	switch (error.code)
	{
		case ErrorCode::FILE_NOT_FOUND:				return std::format("Path '{}' not found", error.info);
		case ErrorCode::BAD_FORMAT_ARGS:			return std::format("Invalid arg format: {}", error.info);
		case ErrorCode::CLIENT_DISCONNECTED:		return std::format("Client HTTP disconnected", error.info);
		case ErrorCode::SERVER_DISCONNECTED:		return std::format("Server disconnected", error.info);
		case ErrorCode::SERVER_CONN_FAILED:			return std::format("Connection to server failed: {}", error.info);
		case ErrorCode::SERVER_ERROR:				return std::format("Server error: {}", error.info);
		case ErrorCode::IO_READ_FAILED:				return std::format("Read error: {}", error.info);
		case ErrorCode::IO_WRITE_FAILED:			return std::format("Write error: {}", error.info);
		case ErrorCode::IO_READ_NONB_FAILED:		return std::format("Recv error: {}", error.info);
		case ErrorCode::IO_WRITE_NONB_FAILED:		return std::format("Send error: {}", error.info);
		case ErrorCode::IO_POLL_FAILED:				return std::format("Poll error: {}", error.info);
		case ErrorCode::IO_PIPE_FAILED:				return std::format("Piping data failed: {}", error.info);
		case ErrorCode::IO_PIPE_CREATE_FAILED:		return std::format("Failed to create pipe: {}", error.info);
		case ErrorCode::IO_SOCK_CREATE_FAILED:		return std::format("Failed to create socket: {}", error.info);
		case ErrorCode::IO_SIG_SOCK_CREATE_FAILED:	return std::format("Failed to create signal redirected fd: {}", error.info);
		case ErrorCode::UI_INVALID_SIZE:			return std::format("UI widget couldn't be drawn (newinw() failed): {}", error.info);
		case ErrorCode::UI_HANDSHAKE_NOT_DONE:		return std::format("No handshake received before performing action", error.info);
		case ErrorCode::UI_USERNAME_NOT_EXISTS:		return std::format("Username doesn't exist", error.info);
		case ErrorCode::UI_USERNAME_IN_USE:			return std::format("Username already in use");
		case ErrorCode::UI_FAILED_GET_TERM_SIZE:	return std::format("Couldn't get terminal size", error.info);
		case ErrorCode::UI_DOUBLE_ERROR:			return std::format("Got error: '{}' while handling one already", error.info);
		default:									return std::format("No error found for code: {}", error.info);
	}
}
