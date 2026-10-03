#include <iostream>
#include <memory>

#include "ArgParser.hpp"
#include "ClientHTTP.hpp"
#include "UI.hpp"
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
	Logger::getInstance().removeFilter(LogContext::INPUT_OUTPUT);
}

int32_t main(int32_t argc, char** argv)
{
	startLogging();

	try
	{
		Flags options = parseArguments(argc, argv);

		if (options.helpmode == true)
		{
			std::cout << HOW_TO << std::endl;
			return (EXIT_SUCCESS);
		}

		std::unique_ptr<UI> app = uiFactory();
		app->connect(options.host, options.port);
		app->start();
	}
	catch (AppException const& err)
	{
		if (err.getError().code == ErrorCode::BAD_FORMAT_ARGS)
		{
			std::cout << HOW_TO << std::endl;
			return (EXIT_SUCCESS);
		}
		else
		{
			std::cerr << "Got error: " << err.what() << std::endl;
			return (EXIT_FAILURE);
		}
	}

	return (EXIT_SUCCESS);
}