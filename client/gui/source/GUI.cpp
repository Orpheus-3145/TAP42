#include "GUI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"


// because I don't want to start the Qt app in main;
// global because they need to exist for the whole session
static int32_t argc = 1;
static char arg0[] = "tap-gui-client";
static char* argv[] = { arg0, nullptr };

GUI::GUI(void) : UI()
{
	this->app = std::make_unique<QApplication>(argc, argv);

	this->readClientNotifier = std::make_unique<QSocketNotifier>(this->gameClientSockets.first, QSocketNotifier::Read, nullptr);
	QObject::connect(
		this->readClientNotifier.get(),
		&QSocketNotifier::activated,
		this,
		&GUI::readFromServer
	);

	this->writeClientNotifier = std::make_unique<QSocketNotifier>(this->gameClientSockets.first, QSocketNotifier::Write, nullptr);
	this->writeClientNotifier->setEnabled(false);
	QObject::connect(
		this->writeClientNotifier.get(),
		&QSocketNotifier::activated,
		this,
		&GUI::writeToServer
	);

	this->loginWin = std::make_unique<LoginWindow>(this);
	this->loginWin->draw();
	QObject::connect(
		loginWin.get(),
		&LoginWindow::commandEntered,
		this,
		&GUI::handleCommand
	);

	this->newPlayerWin = std::make_unique<PlayerCreateWindow>(this);
	this->newPlayerWin->draw();
	QObject::connect(
		newPlayerWin.get(),
		&PlayerCreateWindow::commandEntered,
		this,
		&GUI::handleCommand
	);

	this->gameWin = std::make_unique<GameWindow>(this);
	QObject::connect(
		gameWin.get(),
		&GameWindow::commandEntered,
		this,
		&GUI::handleCommand
	);

	this->errorWin = std::make_unique<ErrorWindow>(this);

	LOG_INFO(LogContext::GUI, "Done setup GUI");
}

GUI::~GUI(void) noexcept
{
	this->readClientNotifier->setEnabled(false);
	this->writeClientNotifier->setEnabled(false);

	// destroy manually to be sure to kill the app last
	this->readClientNotifier.reset();
	this->writeClientNotifier.reset();

	this->loginWin.reset();
	this->newPlayerWin.reset();
	this->gameWin.reset();
	this->errorWin.reset();
}

void GUI::start(void)
{
	UI::start();
	this->app->exec();
}

void GUI::stop(void) noexcept
{
	UI::stop();
	this->app->quit();
}

void GUI::switchWindow(std::optional<GamePhase> newPhase)
{
	UI::switchWindow(newPhase);

	if (this->currentWindow and this->phase != GamePhase::ERROR)
		this->currentWindow->close();

	switch (this->phase)
	{
		case GamePhase::LOGIN:			this->currentWindow = this->loginWin.get(); break;
		case GamePhase::PLAYER_CREATE:	this->currentWindow = this->newPlayerWin.get(); break;
		case GamePhase::GAME:			this->currentWindow = this->gameWin.get(); this->gameWin->draw(); break;		// NB handle re-creation of window
		case GamePhase::ERROR:			this->currentWindow = this->errorWin.get(); this->errorWin->draw(); break;
		default:						assert(false and "phase not mapped to any window");
	}
	this->currentWindow->show();
}

ssize_t GUI::writeToServer(void)
{
	ssize_t n = UI::writeToServer();

	if (this->toServerSize == 0UL)					// written everything, just listen while waiting for server response
		this->writeClientNotifier->setEnabled(false);

	return n;
}

ssize_t GUI::readFromServer(void)
{
	ssize_t n = UI::readFromServer();

	if (this->commandQueue.empty() == false)		// there's another command queued up, send it
		this->writeClientNotifier->setEnabled(true);

	return n;
}

void GUI::shakeHands(void) noexcept
{
	UI::shakeHands();

	if (this->phase == GamePhase::ERROR)
		this->writeClientNotifier->setEnabled(true);
}

void GUI::handleException(ErrorData const& error) noexcept
{
	UI::handleException(error);
	// NB ...
}

void GUI::handleCommand(std::string const& input)
{
	if (this->commandQueue.empty() == true)
		this->writeClientNotifier->setEnabled(true);

	UI::handleCommand(input);
}

void GUI::showResponse(Message const& response) noexcept
{
	assert(!this->commandQueue.empty() and "no command in queue");

	this->gameWin->appendResponse(response, this->commandQueue.front().getCommandType());
}

void GUI::showEvent(Message const& event) noexcept
{
	this->gameWin->appendEvent(event);
}

void GUI::showError(Message const& error) noexcept
{
	assert(!this->commandQueue.empty() and "no command in queue");

	this->gameWin->appendError(error, this->commandQueue.front().getCommandType());
}


std::unique_ptr<UI> uiFactory(void)
{
	return std::make_unique<GUI>();
}
