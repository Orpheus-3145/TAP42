#include <map>
#include <functional>

#include "ArgParser.hpp"
#include "Exceptions.hpp"


static const std::map<std::string, FlagType> flagsMap
{
	{"-i", FlagType::Host},
	{"--host", FlagType::Host},
	{"-p", FlagType::Port},
	{"--port", FlagType::Port},
	{"-h", FlagType::HelpMode},
	{"--help", FlagType::HelpMode}
};

static void setHost( Flags& input, std::string const& hostName )
{
	input.host = hostName;
}

static void setPort( Flags& input, std::string const& port ) {
	try {
		input.port = std::stoul(port);
	} catch (std::invalid_argument const&) {
		throw ParsingException("Wrong number input: " + port);
	} catch (std::out_of_range const&) {
		throw ParsingException("Out of range: " + port);
	}
	if (input.port > 65535)
		throw ParsingException("Port number too big: " + port);
}

static void setHelpMode( Flags& input, std::string const& optValue )
{
	(void)optValue;
	input.helpmode = true;
}

static const std::map<FlagType, std::function<void(Flags&, std::string const&)>> flagActions
{
	{FlagType::Host, setHost},
	{FlagType::Port, setPort},
	{FlagType::HelpMode, setHelpMode},
};

constexpr FlagType MANDATORY_FLAGS = FlagType::Host | FlagType::Port;
constexpr FlagType NO_ARG_FLAGS = FlagType::HelpMode;

Flags parseArguments(int32_t argc, char* argv[])
{
	Flags arguments;
	std::string flagKey, flagValue;
	FlagType flag, checkFlags;
	std::function<void(Flags&, std::string const&)> action;

	if (argc < 2)
		throw ParsingException("Not enough argument provided");

	for (int32_t i = 1; i < argc; ++i)
	{
		flagKey = argv[i];
		if (flagKey[0] != '-')
			throw ParsingException("Flag not well formatted: " + flagKey);

		size_t eqPos = flagKey.find('=');
		if (eqPos != std::string::npos) // if it is --key=value
		{
			if (eqPos == flagKey.size() - 1)
				throw ParsingException("Invalid argument with value: " + flagKey);

			flagValue = flagKey.substr(eqPos + 1);
			flagKey = flagKey.substr(0, eqPos);
		}

		try {
			flag = flagsMap.at(flagKey);
		} catch(const std::out_of_range& e) {
			throw ParsingException("Unknown argument: " + flagKey);
		}
		checkFlags = checkFlags | flag;

		if (eqPos == std::string::npos) // or it is --key value
		{
			if ((flag & NO_ARG_FLAGS) != FlagType::NoFlag)
			{
				flagValue = "";
			}
			else
			{
				if ((i + 1 == argc))
					throw ParsingException("No argument provided for flag: " + flagKey);

				flagValue = argv[++i];
			}
		}

		try {
			action = flagActions.at(flag);
		} catch(const std::out_of_range& e) {
			throw ParsingException("No action linked to: " + flagKey);
		}

		action(arguments, flagValue);
	}

	if ((checkFlags & MANDATORY_FLAGS) != MANDATORY_FLAGS)
		throw ParsingException("Missing mandatory flags");

	return arguments;
}
