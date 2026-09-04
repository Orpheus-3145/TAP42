#pragma once

#include <string>
#include <cstdint>

#include "ClientHTTP.hpp"
#include "UI.hpp"


class Game
{
	public:
		Game(void) noexcept = default;
		
		Game(Game const&) = delete;
		Game& operator=(Game const&) = delete;
		Game(Game&&) = delete;
		Game& operator=(Game&&) = delete;

		~Game(void) = default;

		void start(std::string const& host, uint32_t port);
		
	private:
		ClientHTTP 		clientHTTP{};
		CommandLineUI	interface{};		// later on might be a pointer for doing poly stuff
};