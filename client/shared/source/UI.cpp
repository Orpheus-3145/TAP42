#include "UI.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

#include <cstring>
#include <cassert>
#include <format>


std::string toString(GamePhase phase)
{
	switch (phase)
	{
		case GamePhase::LOGIN: 			return "login";
		case GamePhase::PLAYER_CREATE:	return "character creation";
		case GamePhase::GAME:			return "game";
		case GamePhase::ERROR:			return "error";
		case GamePhase::ND:				return "unset";
		default:						return "";
	}
}

std::ostream& operator<<(std::ostream& out, GamePhase phase)
{
	out << toString(phase);
	return out;
}


UI::UI(void) noexcept : gameClientSockets{ioUtils::createSocketPair()}
{
	this->setupDispatchers();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Listening to client socket: {}", this->gameClientSockets.first));
}

UI::~UI(void) noexcept
{
	if (this->clientHTTP)
		this->clientHTTP->stopWorker();

	ioUtils::closePair(this->gameClientSockets);
}

void UI::connect(std::string host, uint32_t port)
{
	this->clientHTTP = std::make_unique<ClientHTTP>(host, port, this->gameClientSockets.second);
}

void UI::start(void)
{
	this->clientHTTP->startWorker();
	// add an artifacted command whose response is gonna be the handshake from server
	this->commandQueue.push(Message(toString(CommandType::WAIT_HANDSHAKE)));
}

void UI::stop(void) noexcept
{
	this->clientHTTP->stopWorker();
}

void UI::switchWindow(std::optional<GamePhase> newPhase)
{
	this->updatePhase(newPhase);

	// fetch character basic data when session starts
	if (this->phase == GamePhase::GAME)
		this->commandQueue.push(Message("PROFILE"));
}

void UI::updatePhase(std::optional<GamePhase> newPhase)
{
	// without argument switch to
	// the window shown before the error happened
	if (newPhase.has_value() == false)
	{
		assert(this->lastPhase != GamePhase::ND and "last phase was not set");
		newPhase = this->lastPhase;
	}

	if ((this->connEstablished == false) and
		(newPhase.value() != GamePhase::LOGIN) and
		(newPhase.value() != GamePhase::ERROR))		// this is because technically there could a disconnection before login
		throw AppException(ErrorCode::UI_HANDSHAKE_NOT_DONE);

	if (newPhase.value() == GamePhase::ERROR)		// save this phase to restore it when the error has been solved
		this->lastPhase = this->phase;
	else if (this->phase != GamePhase::ERROR)		// careful not to overwrite the state in case of double error
		this->lastPhase = GamePhase::ND;

	if (newPhase.value() == GamePhase::ERROR)		// error win: keep listening server data but stop talking
		this->stopSpeaking();
	else if (this->toServerSize > 0UL)				// if recovering from error and there's data left, start talking
		this->startSpeaking();

	this->phase = newPhase.value();
	LOG_DEBUG(LogContext::INTERFACE, std::format("Switching to: {} window", toString(this->phase)));
}

void UI::writeToServer(void)
{
	if (this->connEstablished == false)
		throw AppException(ErrorCode::UI_HANDSHAKE_NOT_DONE, "Tried to send data without handshake");

	if (this->toServerSize == 0UL)		// if buffer is empty send the next command in queue
	{
		assert(this->commandQueue.empty() == false and "Received write event with an empty command queue");
		std::string nextCommand = this->commandQueue.front().getRawMessage();
		::memcpy(this->toServerBuffer, nextCommand.data(), nextCommand.size());
		this->toServerSize = nextCommand.size();
		this->toServerBuffer[this->toServerSize++] = Config::COMMAND_TERM;
	}

	ssize_t n = ioUtils::writeNonBlock(this->gameClientSockets.first, this->toServerBuffer, this->toServerSize);
	if (n < 0L)
		throw AppException({ErrorCode::CLIENT_DISCONNECTED});
	else if (n > 0L)
	{
		LOG_DEBUG(LogContext::INTERFACE, std::format("Sent to client: '{}'", std::string(this->toServerBuffer, n)));

		this->toServerSize -= n;
		if (this->toServerSize > 0UL)		// move the remaining data to send at the beginning of the buffer
			::memmove(this->toServerBuffer, this->toServerBuffer + n, this->toServerSize);
		else
			this->stopSpeaking();
	}
	else
		LOG_WARN(LogContext::INTERFACE, "Client socket buffer is busy, try again later");
}

void UI::readFromServer(void)
{
	ssize_t n = ioUtils::readNonBlock(this->gameClientSockets.first, this->toGameBuffer + this->toGameSize, Config::BUFF_SIZE - this->toGameSize);
	if (n < 0L)
		throw AppException({ErrorCode::CLIENT_DISCONNECTED});
	else if (n > 0L)
	{
		std::string serverData = std::string(this->toGameBuffer + this->toGameSize, n);
		LOG_DEBUG(LogContext::INTERFACE, std::format("Read from client: '{}'", serverData));

		this->toGameSize += n;
		this->splitIntoMessages();
		if (this->commandQueue.empty() == false)
			this->startSpeaking();
	}
	else
		LOG_WARN(LogContext::INTERFACE, "Client socket buffer is full, send data to UI and empty it");
}

void UI::splitIntoMessages(void)
{
	while (true)
	{
		char *endMsg = reinterpret_cast<char*>(::memchr(this->toGameBuffer, Config::COMMAND_TERM, this->toGameSize));
		if (endMsg == nullptr)
			break;

		ssize_t lenMsg = endMsg - this->toGameBuffer;
		std::string msg = std::string(this->toGameBuffer, lenMsg);

		this->toGameSize -= lenMsg + 1UL;
		::memmove(this->toGameBuffer, this->toGameBuffer + lenMsg + 1UL, this->toGameSize);
		if (lenMsg == 0L)
			continue;

		this->dispatchMessage(msg);
	}
}

void UI::dispatchMessage(std::string const& msg)
{
	std::unique_ptr<Message> message;
	try
	{
		message = std::make_unique<Message>(msg);
	}
	catch (AppException const& e)
	{
		if (e.getError().code != ErrorCode::BAD_MESSAGE)
			throw e;

		LOG_ERROR(LogContext::INTERFACE, std::format("Parsing error in server data: {} - discarded", mapError(e.getError())));
		return;
	}

	if (message->isResponse())
	{
		if (this->commandQueue.empty() == false)
		{
			this->responseDispatcher.at(this->phase)(*message);
			if (this->commandQueue.front().getCommandType() == CommandType::QUIT)
				this->stop();

			this->commandQueue.pop();
		}
		else
			LOG_ERROR(LogContext::INTERFACE, std::format("Got response without any pending command: '{}' - discarded", message->getRawMessage()));
	}
	else if (message->isError())
	{
		if (message->getErrorType() == ErrorType::CONNECTION_FAILED)
			throw AppException(ErrorCode::SERVER_DISCONNECTED);
		else if (this->commandQueue.empty() == false)
		{
			this->errorDispatcher.at(this->phase)(*message);
			this->commandQueue.pop();
		}
		else
			LOG_ERROR(LogContext::INTERFACE, std::format("Got error without any pending command: '{}' - discarded", message->getRawMessage()));
	}
	else if (message->isEvent())
	{
		this->eventDispatcher.at(this->phase)(*message);
	}
}

void UI::shakeHands(void) noexcept
{
	this->connEstablished = true;
	LOG_DEBUG(LogContext::INTERFACE, "Handshake performed");

	if (this->phase == GamePhase::ND)				// fresh new conn
		this->switchWindow(GamePhase::LOGIN);
	else if ((this->phase == GamePhase::ERROR))		// disconnected, restore previous session
	{
		if (this->info.username.empty() == false)
		{
			// forward the previously used username to server
			std::string loginCommand = std::format("{} {}", toString(CommandType::CONNECT), this->info.username);
			this->commandQueue.push(Message(loginCommand));
			this->startSpeaking();
		}
		else	// no username, disconnection happened during login/char creation switch back to that window
			this->switchWindow();
	}
}

void UI::handleException(ErrorData const& error)
{
	LOG_ERROR(LogContext::INTERFACE, std::format("Error: {}", mapError(error)));

	if (error.code == ErrorCode::SERVER_DISCONNECTED)
	{
		while (this->commandQueue.empty() == false)
			this->commandQueue.pop();
		this->toGameSize = 0UL;
		this->toServerSize = 0UL;

		this->commandQueue.push(Message(toString(CommandType::WAIT_HANDSHAKE)));
		this->connEstablished = false;
	}
	this->switchWindow(GamePhase::ERROR);
}

void UI::handleCommand(std::string const& input)
{
	std::string command;
	if (this->phase == GamePhase::LOGIN)
	{
		if (this->commandQueue.empty() == false)	// to avoid sending multiple requests for connections
			this->commandQueue.pop();

		command = std::format("{} {}", toString(CommandType::CONNECT), input);
	}
	else if (this->phase == GamePhase::PLAYER_CREATE)
	{
		if (this->commandQueue.empty() == false)	// to avoid sending multiple requests for connections
			this->commandQueue.pop();

		command = std::format("{} {}", toString(CommandType::CREATE_PLAYER), input);
	}
	else
		command = input;

	this->commandQueue.push(Message(command));
	LOG_DEBUG(LogContext::INTERFACE, std::format("Command '{}' added to queue", command));

	// send this command if it's the first in queue, otherwise there's
	// one being already handled
	if (this->commandQueue.size() == 1UL)
		this->startSpeaking();
}

void UI::setupDispatchers(void)
{
	this->responseDispatcher[GamePhase::ND] = 			 [this] (Message const& m) { (void) m;/* NB validate */ this->shakeHands(); };
	this->responseDispatcher[GamePhase::LOGIN] =		 [this] (Message const& m) { (void) m;/* NB validate */ this->switchWindow(GamePhase::GAME); };
	this->responseDispatcher[GamePhase::PLAYER_CREATE] = [this] (Message const& m) { (void) m;/* NB validate */ this->switchWindow(GamePhase::GAME); };
	this->responseDispatcher[GamePhase::GAME] =			 [this] (Message const& m) {
		
		this->showResponse(m);
	};
	this->responseDispatcher[GamePhase::ERROR] =		 [this] (Message const& m) {
		if (this->commandQueue.front().getCommandType() == CommandType::WAIT_HANDSHAKE)
			this->shakeHands();
		else if (this->commandQueue.front().getCommandType() == CommandType::CONNECT)
			this->switchWindow();
		else
			LOG_WARN(LogContext::INTERFACE, std::format("Got response while handling error: '{}' - discarded", m.getRawMessage()));
	};

	this->eventDispatcher[GamePhase::ND] = 			  [this] (Message const& m) { LOG_WARN(LogContext::INTERFACE, std::format("Got unexpected event before handshake: '{}' - discarded", m.getRawMessage())); };
	this->eventDispatcher[GamePhase::LOGIN] =		  [this] (Message const& m) { LOG_WARN(LogContext::INTERFACE, std::format("Got unexpected event during login: '{}' - discarded", m.getRawMessage())); };
	this->eventDispatcher[GamePhase::PLAYER_CREATE] = [this] (Message const& m) { LOG_WARN(LogContext::INTERFACE, std::format("Got unexpected event during player creation: '{}' - discarded", m.getRawMessage())); };
	this->eventDispatcher[GamePhase::GAME] =		  [this] (Message const& m) { this->showEvent(m); };
	this->eventDispatcher[GamePhase::ERROR] =		  [this] (Message const& m) { LOG_WARN(LogContext::INTERFACE, std::format("Got event while handling error: '{}' - discarded", m.getRawMessage())); };		// add response to a queue, to be shown when game starts again NB

	this->errorDispatcher[GamePhase::ND] = 			  [this] (Message const& m) { LOG_WARN(LogContext::INTERFACE, std::format("Got unexpected error before handshake: '{}' - discarded", m.getRawMessage())); };
	this->errorDispatcher[GamePhase::LOGIN] =		  [this] (Message const& m) {
		if (m.getErrorType() == ErrorType::CHARACTER_NOT_FOUND)
			throw AppException(ErrorCode::UI_USERNAME_NOT_EXISTS);
		else
			throw AppException(ErrorCode::SERVER_ERROR, m.toStringError());
	};
	this->errorDispatcher[GamePhase::PLAYER_CREATE] = [this] (Message const& m) {
		if (m.getErrorType() == ErrorType::NAME_IN_USE)
			throw AppException(ErrorCode::UI_USERNAME_IN_USE);
		else
			throw AppException(ErrorCode::SERVER_ERROR, m.toStringError());
	};
	this->errorDispatcher[GamePhase::GAME] =		  [this] (Message const& m) { this->showError(m); };
	this->errorDispatcher[GamePhase::ERROR] =		  [this] (Message const& m) { throw AppException(ErrorCode::SERVER_ERROR, m.getRawMessage()); };
}
