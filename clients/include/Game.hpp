#pragma once

#include <string>
#include <cstdint>

#include "ClientHTTP.hpp"
#include "CLI.hpp"


inline constexpr const char* PROMPT = "=> ";
inline constexpr const char* GAME_NAME = "__TBD_NAME__";

class Game
{
	public:
		Game(void);
		~Game(void);

		Game(Game const&) = delete;
		Game& operator=(Game const&) = delete;
		Game(Game&&) = delete;
		Game& operator=(Game&&) = delete;

		void connectToServer(std::string const& host, uint32_t port);
		void gameLoop(void);
		
	private:
		void printInfo(void) noexcept;
		void printAsync(std::string const& content) noexcept;

		std::mutex              	printMutex;
		std::unique_ptr<ClientHTTP> clientHTTP;
		std::unique_ptr<CLI>		interface;
};