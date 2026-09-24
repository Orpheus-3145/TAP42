#include "GUI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"


// because I don't want to start the Qt app in main;
// global because they need to exist for the whole session
static int32_t argc = 1;
static char arg0[] = "tap-gui-client";
static char* argv[] = { arg0, nullptr };

GUI::GUI(int32_t clientSocket, int32_t height, int32_t width) :
	UI(clientSocket)
{
	this->app = std::make_unique<QApplication>(argc, argv);
	this->gameWin = std::make_unique<GameWindow>(nullptr, height, width);

	// when user sends a command from the UI
	QObject::connect(
		this->gameWin.get(),
		&GameWindow::commandEntered,
	    this,
		[this](const QString &cmd) {
			std::string cmdStr = cmd.toStdString();
			LOG_DEBUG(LogContext::INTERFACE, std::format("got: '{}'", cmdStr));
			this->toServerSize = cmdStr.size();
			::memcpy(this->toServerBuffer, cmdStr.data(), this->toServerSize);
			this->writeToServer();
		}
	);

	// server data: responses, events
	this->readFromServerNotifier = std::make_unique<QSocketNotifier>(this->clientSocket, QSocketNotifier::Read, nullptr);
	QObject::connect(
		this->readFromServerNotifier.get(),
		&QSocketNotifier::activated,
	    this,
		[this]() {
			this->readFromServer();
		}
	);

    this->gameWin->show();
}

GUI::~GUI(void) noexcept
{
	if (this->readFromServerNotifier)
		this->readFromServerNotifier->setEnabled(false);

	// destroy manually to be sure to kill the app last
	this->gameWin.reset();
	this->app.reset();
}

void GUI::showResponse(std::string const& response) noexcept
{
	this->gameWin->appendResponse(QString::fromStdString(response));
}

void GUI::showEvent(std::string const& event) noexcept
{
	this->gameWin->appendEvent(QString::fromStdString(event));
}

std::unique_ptr<UI> uiFactory(int32_t clientSocket)
{
	return std::make_unique<GUI>(clientSocket, Config::HEIGHT_GUI, Config::WIDTH_GUI);
}
