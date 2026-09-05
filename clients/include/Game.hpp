#pragma once

#include <string>
#include <cstdint>

#include "ClientHTTP.hpp"
#include "UI.hpp"


class Game
{
	public:
		Game(void) noexcept;
		
		Game(Game const&) = delete;
		Game& operator=(Game const&) = delete;
		Game(Game&&) = delete;
		Game& operator=(Game&&) = delete;

		~Game(void);

		void start(std::string const& host, uint32_t port);
		void stop(void) noexcept;

	private:
		void loop(int32_t clientSocket);
		void forwardCommandToServer(int32_t clientSocket);
		void readDataFromServer(int32_t clientSocket);
		void handleServerInput(void);

		ioUtils::Pipe commandPipe;

		std::unique_ptr<ClientHTTP> 	clientHTTP;
		std::unique_ptr<CommandLineUI>	interface;		// later on might be a pointer for doing poly stuff

		bool runLoop{false};

		size_t	serverInputLength{0UL};
		char	serverBuffer[Config::R_BUFF_SIZE];

		size_t	commandLength{0UL};
		char	commandBuffer[Config::R_BUFF_SIZE];
};
