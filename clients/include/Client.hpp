#pragma once

#include <string>
#include <cstdint>
#include <netdb.h> 		// gai_strerror, getaddrinfo, freeaddrinfo


struct Address
{
	std::string 			host;
	uint32_t				port{0U};
	struct sockaddr_storage rawAddress{};
};

Address	getAddress(const struct sockaddr_storage*) noexcept;

class Client
{
	public:
		Client(void) noexcept;
		Client(Client const&) = delete;
		Client& operator=(Client const&) = delete;
		Client(Client&&) noexcept;
		Client& operator=(Client&&) noexcept;
		~Client(void);

		void	connect(std::string const& host, uint32_t port);
		void	sendRequest(std::string const& content) const;
		int32_t	getSocket(void) const noexcept { return this->socket; }

	private:
		int32_t			socket{-1};
		Address		 	serverAddress{};
		struct addrinfo	filter{};
};
