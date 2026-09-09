#pragma once

#include <cstdint>
#include <string>


constexpr const char* HOW_TO = R"(Usage: ./client [flags]

	-i,  --host     IP address of the server to connect
	-p,  --port     port to connect
	-h,  --help     print info

	[flags can be set in two ways: --flag value  | --flag=value ]
)";

struct Flags
{
	std::string host;
	uint32_t	port{0U};
	bool		helpmode{false};
};

enum class FlagType : uint32_t
{
	NoFlag = 0,
	Host = 1 << 0,
	Port = 1 << 2,
	HelpMode = 1 << 3,
};

constexpr bool operator==(FlagType a, FlagType b) noexcept
{
	return static_cast<uint32_t>(a) == static_cast<uint32_t>(b);
}

constexpr bool operator!=(FlagType a, FlagType b) noexcept
{
	return static_cast<uint32_t>(a) != static_cast<uint32_t>(b);
}

constexpr FlagType operator|(FlagType a, FlagType b) noexcept
{
	return static_cast<FlagType>(
		static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
	);
}

constexpr FlagType operator&(FlagType a, FlagType b) noexcept
{
	return static_cast<FlagType>(
		static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
	);
}

Flags parseArguments(int32_t argc, char* argv[]);
