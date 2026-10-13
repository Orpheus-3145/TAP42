#include "GUI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"


// because I don't want to start the Qt app in main;
// global because they need to exist for the whole session
static int32_t argc = 1;
static char arg0[] = "tap-gui-client";
static char* argv[] = { arg0, nullptr };

GUI::GUI(int32_t height, int32_t width) : UI()
{
	this->app = std::make_unique<QApplication>(argc, argv);

	// server data: responses, events
	this->readFromServerNotifier = std::make_unique<QSocketNotifier>(this->pollFds[GUI::CLIENT].fd, QSocketNotifier::Read, nullptr);
	QObject::connect(
		this->readFromServerNotifier.get(),
		&QSocketNotifier::activated,
	    this,
		[this]() {
			this->readFromServer();
		}
	);

	this->loginWin = std::make_unique<LoginWindow>(nullptr, this, height, width);
	this->newPlayerWin = std::make_unique<PlayerCreateWindow>(nullptr, this, height, width);
	this->gameWin = std::make_unique<GameWindow>(nullptr, this, height, width);
	this->errorWin = std::make_unique<ErrorWindow>(nullptr, this, height, width);

	// when user sends a command from the UI
	QObject::connect(
		this->gameWin.get(),
		&GameWindow::commandEntered,
	    this,
		[this](const QString &cmd) {
			std::string cmdStr = cmd.toStdString();
			this->toServerSize = cmdStr.size();
			::memcpy(this->toServerBuffer, cmdStr.data(), this->toServerSize);
			this->writeToServer();
		}
	);
    this->gameWin->show();
	LOG_INFO(LogContext::INTERFACE, "Done setup GUI");
}

GUI::~GUI(void) noexcept
{
	if (this->readFromServerNotifier)
		this->readFromServerNotifier->setEnabled(false);

	// destroy manually to be sure to kill the app last
	this->gameWin.reset();
	this->app.reset();
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

	switch (this->phase)
	{
		case GamePhase::LOGIN:			this->currentWindow = this->loginWin.get(); break;
		case GamePhase::PLAYER_CREATE:	this->currentWindow = this->newPlayerWin.get(); break;
		case GamePhase::GAME:			this->currentWindow = this->gameWin.get(); break;
		case GamePhase::ERROR:			this->currentWindow = this->errorWin.get(); break;
		default:						assert(false and "phase not mapped to any window");
	}
	this->currentWindow->show();
}

void GUI::showResponse(Message const& response) noexcept
{
	this->gameWin->appendResponse(response, this->commandQueue.front().getCommandType());
	// this->gameWin->appendResponse(QString::fromStdString(response.));
}

void GUI::showEvent(Message const& event) noexcept
{
	this->gameWin->appendEvent(event);
	// this->gameWin->appendEvent(QString::fromStdString(event.));
}

void GUI::showError(Message const& error) noexcept
{
	this->gameWin->appendResponse(error, this->commandQueue.front().getCommandType());
	// this->gameWin->appendResponse(QString::fromStdString(error));
}


std::unique_ptr<UI> uiFactory(void)
{
	return std::make_unique<GUI>(Config::HEIGHT_GUI, Config::WIDTH_GUI);
}
