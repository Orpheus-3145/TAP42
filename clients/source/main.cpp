#include <iostream>

#include "Exceptions.hpp"
#include "ArgParser.hpp"
#include "GameCLI.hpp"


int32_t main(int32_t argc, char** argv)
{
	try {
		Flags options = parseArguments(argc, argv);
		if (options.helpmode == true) {
			std::cout << HOW_TO << std::endl;
			return (EXIT_SUCCESS);
		}

		GameCLI client;

		client.connectToServer(options.host, options.port);
		client.send("halo porcoddio");
	} catch (AppException& err) {
		std::cerr << err.what() << std::endl;
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}