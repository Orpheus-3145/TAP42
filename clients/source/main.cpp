#include <iostream>
#include <queue>

#include "Exceptions.hpp"
#include "ArgParser.hpp"
#include "Game.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Config.hpp"


void startLogging(void)
{
	std::string logName = createLogPath(Config::LOG_DIR);
	Logger::getInstance().setLogFile(logName);
	Logger::getInstance().setMinLevel(Config::DEFAULT_LOG_LEVEL);
	Logger::getInstance().setConsoleOutput(false);
}

int32_t main(int32_t argc, char** argv)
{
	try {
		startLogging();

		Flags options = parseArguments(argc, argv);
		if (options.helpmode == true) {
			std::cout << HOW_TO << std::endl;
			return (EXIT_SUCCESS);
		}

		Game client;
		client.start(options.host, options.port);

	} catch (AppException& err) {
		std::cerr << err.what() << std::endl;
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}