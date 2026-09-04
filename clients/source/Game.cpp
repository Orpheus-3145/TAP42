#include "Game.hpp"
#include "Utils.hpp"


void Game::start(std::string const& host, uint32_t port)
{
	ioUtils::SocketPair gameClientSockets = ioUtils::createSocketPair();

	this->clientHTTP.connect(host, port);
	this->clientHTTP.startWorker(gameClientSockets.first);
	
	this->interface.setup();
	this->interface.loop(gameClientSockets.second);
	this->interface.stop(gameClientSockets.second);
}
