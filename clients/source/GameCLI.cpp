#include <cassert>

#include "GameCLI.hpp"
#include "Exceptions.hpp"


GameCLI::GameCLI(void)
{
	this->clientHTTP = std::make_unique<ClientHTTP>();
}

GameCLI::~GameCLI(void)
{
	assert(this->clientHTTP and "client not existing");

	this->clientHTTP->disconnect();
}

void GameCLI::connectToServer(std::string const& host, uint32_t port)
{
	assert(this->clientHTTP and "client not existing");

	this->clientHTTP->connect(host, port);
}

void GameCLI::send(std::string const& data)
{
	assert(this->clientHTTP and "client not existing");

	this->clientHTTP->sendRequest(data);
}
