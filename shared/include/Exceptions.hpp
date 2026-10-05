#pragma once

#include <string>
#include <cstdint>
#include <stdexcept>


enum class ErrorCode : uint32_t
{
	FILE_NOT_FOUND = 0U,
	BAD_FORMAT_ARGS,

	CLIENT_DISCONNECTED,
	SERVER_DISCONNECTED,
	SERVER_CONN_FAILED,
	SERVER_ERROR,
	BAD_MESSAGE,

	IO_READ_FAILED,
	IO_WRITE_FAILED,
	IO_READ_NONB_FAILED,
	IO_WRITE_NONB_FAILED,
	IO_POLL_FAILED,
	IO_PIPE_FAILED,
	IO_PIPE_CREATE_FAILED,
	IO_SOCK_CREATE_FAILED,
	IO_SIG_SOCK_CREATE_FAILED,

	UI_INVALID_SIZE,
	UI_HANDSHAKE_NOT_DONE,
	UI_USERNAME_NOT_EXISTS,
	UI_USERNAME_IN_USE,
	UI_FAILED_GET_TERM_SIZE,
	UI_DOUBLE_ERROR
};

struct ErrorData
{
	ErrorCode	code;
	std::string	info;
};

std::string mapError(ErrorData const& error) noexcept;

class AppException : public std::runtime_error {
	public:
		AppException(ErrorData const& error) noexcept :
			std::runtime_error(mapError(error)),
			errData{error} {}
		AppException(ErrorCode code, std::string const& info = "") noexcept :
			std::runtime_error(mapError({code, info})),
			errData{ErrorData{code, info}} {}

		ErrorData const& getError(void) const noexcept { return this->errData; }

	private:
		ErrorData errData;
};
