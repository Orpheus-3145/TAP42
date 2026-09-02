#pragma once

#include <string>
#include <cstdint>

#include "ClientHTTP.hpp"


class GameGUI
{
	public:
		GameGUI(void);
		~GameGUI(void);

		GameGUI(GameGUI const&) = delete;
		GameGUI& operator=(GameGUI const&) = delete;
		GameGUI(GameGUI&&) = delete;
		GameGUI& operator=(GameGUI&&) = delete;

		void connectToServer(std::string const& host, uint32_t port);
		void send(std::string const& data);

	private:
		std::unique_ptr<ClientHTTP> clientHTTP;
};