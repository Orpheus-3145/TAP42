#include <iostream>

#include "ArgParser.hpp"
#include "ClientHTTP.hpp"
#include "CLI.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Config.hpp"
#include "Exceptions.hpp"


void startLogging(void)
{
	std::string logName = createLogPath(Config::LOG_DIR);
	Logger::getInstance().setLogFile(logName);
	Logger::getInstance().setMinLevel(Config::DEFAULT_LOG_LEVEL);
	Logger::getInstance().setConsoleOutput(false);
	Logger::getInstance().setFilter(Logger::ALL_ENTRIES & ~LogContext::INPUT_OUTPUT);
}

void run(std::string const& host, uint32_t port)
{
	ioUtils::SocketPair gameClientSockets = ioUtils::createSocketPair();

	ClientHTTP clientHTTP = ClientHTTP(host, port);
	clientHTTP.startWorker(gameClientSockets.first);

	// decide if use CLI or GUI
	CLI interface = CLI(gameClientSockets.second);
	
	interface.loop();		// blocks here, NB if exceptions happen here they must be caught and terminate the running threads

	clientHTTP.stopWorker();
	clientHTTP.disconnect();

	ioUtils::closePair(gameClientSockets);
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

		run(options.host, options.port);

	} catch (AppException& err) {
		std::cerr << err.what() << std::endl;
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}