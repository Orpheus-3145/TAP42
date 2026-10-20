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
	this->stopTalking();
	QObject::connect(
		this->writeClientNotifier.get(),
		&QSocketNotifier::activated,
		this,
		&GUI::writeToServer
	);

	this->loginWin = std::make_unique<LoginWindow>(this);
	this->loginWin->draw();
	QObject::connect(
		this->loginWin.get(),
		&LoginWindow::commandEntered,
		this,
		[this](std::string const& cmd) { this->handleCommand(cmd); }
	);

	this->newPlayerWin = std::make_unique<PlayerCreateWindow>(this);
	this->newPlayerWin->draw();
	QObject::connect(
		this->newPlayerWin.get(),
		&PlayerCreateWindow::commandEntered,
		this,
		[this](std::string const& cmd) { this->handleCommand(cmd); }
	);

	this->gameWin = std::make_unique<GameWindow>(this);
	QObject::connect(
		this->gameWin.get(),
		&GameWindow::commandEntered,
		this,
		[this](std::string const& cmd) { this->handleCommand(cmd); }
	);

	this->errorWin = std::make_unique<ErrorWindow>(this);
	this->errorWin->draw();

	LOG_INFO(LogContext::GUI, "Done setup GUI");
}

GUI::~GUI(void) noexcept
{
	this->readClientNotifier->setEnabled(false);
	this->stopTalking();

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

	if (this->currentWindow)
		this->currentWindow->close();

	switch (this->phase)
	{
		case GamePhase::LOGIN:			this->currentWindow = this->loginWin.get(); break;
		case GamePhase::PLAYER_CREATE:	this->currentWindow = this->newPlayerWin.get(); break;
		case GamePhase::GAME:			this->currentWindow = this->gameWin.get(); this->gameWin->draw(); break;
		case GamePhase::ERROR:			this->currentWindow = this->errorWin.get(); break;
		default:						assert(false and "phase not mapped to any window");
	}
	QRect area = QGuiApplication::primaryScreen()->availableGeometry();
	this->currentWindow->move(area.center() - this->currentWindow->rect().center());

	this->currentWindow->setEnabled(true);
	this->currentWindow->show();
}

void GUI::writeToServer(void)
{
	try
	{
		UI::writeToServer();
	}
	catch (AppException const& e)
	{
		ErrorData err = e.getError();
		this->handleException(err);
	}
}

void GUI::readFromServer(void)
{
	try
	{
		UI::readFromServer();
	}
	catch (AppException const& e)
	{
		ErrorData err = e.getError();
		this->handleException(err);
	}
}

void GUI::handleException(ErrorData const& error) noexcept
{
	UI::handleException(error);

	if (this->phase == GamePhase::ERROR)
		this->errorWin->adaptWinToError({ErrorCode::UI_DOUBLE_ERROR, mapError(error)});
	else
		this->errorWin->adaptWinToError(error);

	this->switchWindow(GamePhase::ERROR);
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
