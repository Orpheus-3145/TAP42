#pragma once

#include <string>
#include <cstdint>

#include "ClientHTTP.hpp"


inline constexpr const char* PROMPT = "=> ";
inline constexpr const char* GAME_NAME = "__TBD_NAME__";

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
		void gameLoop(void);
		
	private:
		void printInfo(void) noexcept;
		void printAsync(std::string const& content) noexcept;

		std::mutex              	printMutex;
		std::unique_ptr<ClientHTTP> clientHTTP;
};