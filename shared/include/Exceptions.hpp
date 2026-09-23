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
	UI_FAILED_GET_TERM_SIZE = 18U,
};


std::string mapError(ErrorCode code, std::string const& errorData) noexcept;

class AppException : public std::runtime_error {
	public:
		explicit AppException(ErrorCode code, std::string const& errorData = "") noexcept;

		ErrorCode	getCode(void) const noexcept { return this->code; }

	private:
		ErrorCode code;
};
