#pragma once

#include <string>
#include <cstdint>

#include "ClientHTTP.hpp"


class GameCLI
{
	public:
		GameCLI(void);
		~GameCLI(void);

		GameCLI(GameCLI const&) = delete;
		GameCLI& operator=(GameCLI const&) = delete;
		GameCLI(GameCLI&&) = delete;
		GameCLI& operator=(GameCLI&&) = delete;

		void connectToServer(std::string const& host, uint32_t port);
		void send(std::string const& data);

	private:
		std::unique_ptr<ClientHTTP> clientHTTP;
};