#include <cassert>

#include "GameGUI.hpp"
#include "Exceptions.hpp"


GameGUI::GameGUI(void)
{
	this->clientHTTP = std::make_unique<ClientHTTP>();
}

GameGUI::~GameGUI(void)
{
	assert(this->clientHTTP and "client not existing");

	this->clientHTTP->disconnect();
}

void GameGUI::connectToServer(std::string const& host, uint32_t port)
{
	assert(this->clientHTTP and "client not existing");

	this->clientHTTP->connect(host, port);
}

void GameGUI::send(std::string const& data)
{
	assert(this->clientHTTP and "client not existing");

	this->clientHTTP->sendRequest(data);
}
