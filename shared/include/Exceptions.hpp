#pragma once

#include <string>
#include <cstdint>
#include <stdexcept>


enum class ErrorCode : uint32_t
{
	FILE_NOT_FOUND = 0U,
	BAD_FORMAT_ARGS = 1U,

	CLIENT_DISCONNECTED = 2U,
	SERVER_DISCONNECTED = 3U,
	SERVER_CONN_FAILED = 4U,
	SERVER_ERROR = 5U,

	IO_READ_FAILED = 6U,
	IO_WRITE_FAILED = 7U,
	IO_READ_NONB_FAILED = 8U,
	IO_WRITE_NONB_FAILED = 9U,
	IO_POLL_FAILED = 10U,
	IO_PIPE_FAILED = 11U,
	IO_PIPE_CREATE_FAILED = 12U,
	IO_SOCK_CREATE_FAILED = 13U,
	IO_SIG_SOCK_CREATE_FAILED = 14U,

	UI_INVALID_SIZE = 15U,
	UI_HANDSHAKE_NOT_DONE = 16U,
	UI_USERNAME_NOT_EXISTS = 17U,
	UI_USERNAME_IN_USE = 18U,
	UI_FAILED_GET_TERM_SIZE = 19U,
	UI_DOUBLE_ERROR = 20U,
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
		AppException(ErrorCode	code, std::string const& info = "") noexcept :
			std::runtime_error(mapError({code, info})),
			errData{ErrorData{code, info}} {}

		ErrorData const&	getError(void) const noexcept { return this->errData; }
		ErrorData&			getError(void) noexcept { return this->errData; }

	private:
		ErrorData errData;
};
