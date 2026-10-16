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

	this->readClientNotifier = std::make_unique<QSocketNotifier>(this->pollFds[GUI::CLIENT].fd, QSocketNotifier::Read, nullptr);
	QObject::connect(
		this->readClientNotifier.get(),
		&QSocketNotifier::activated,
		this,
		&GUI::readFromServer
	);

	this->writeClientNotifier = std::make_unique<QSocketNotifier>(this->pollFds[GUI::CLIENT].fd, QSocketNotifier::Write, nullptr);
	this->writeClientNotifier->setEnabled(false);
	QObject::connect(
		this->writeClientNotifier.get(),
		&QSocketNotifier::activated,
		this,
		&GUI::writeToServer
	);

	this->loginWin = std::make_unique<LoginWindow>(height, width, this);
	this->newPlayerWin = std::make_unique<PlayerCreateWindow>(height, width, this);
	this->gameWin = std::make_unique<GameWindow>(height, width, this);
	this->errorWin = std::make_unique<ErrorWindow>(height, width, this);

	// for (Window *w : {
	// 	static_cast<Window*>(loginWin.get()),
	// 	static_cast<Window*>(newPlayerWin.get()),
	// 	static_cast<Window*>(gameWin.get()),
	// 	static_cast<Window*>(errorWin.get())
	// })
		QObject::connect(
			gameWin.get(),
			&GameWindow::commandEntered,
			this,
			&GUI::handleCommand
		);

	LOG_INFO(LogContext::INTERFACE, "Done setup GUI");
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

	if (this->currentWindow)
		this->currentWindow->close();

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

void GUI::writeToServer(void)
{
	try
	{
		UI::writeToServer();
		if (this->pollFds[GUI::CLIENT].revents & POLLHUP)
			throw AppException({ErrorCode::CLIENT_DISCONNECTED});
		else if (this->pollFds[GUI::CLIENT].events & POLLIN)
			this->writeClientNotifier->setEnabled(false);
	}
	catch (AppException const& e)
	{
		this->handleException(e.getError());
	}
}

void GUI::readFromServer(void)
{
	try
	{
		UI::readFromServer();
		if (this->pollFds[GUI::CLIENT].revents & POLLHUP)
			throw AppException({ErrorCode::CLIENT_DISCONNECTED});
		else if (this->pollFds[GUI::CLIENT].events & POLLOUT)
			this->writeClientNotifier->setEnabled(true);
	}
	catch (AppException const& e)
	{
		this->handleException(e.getError());
	}
}

void GUI::handleException(ErrorData const& error) noexcept
{
	UI::handleException(error);
	// NB ...
}

void GUI::handleCommand(std::string const& input)
{
	// NB move this check in the panels
	// try
	// {
	// 	Message testParse(input);
	// 	// check 1. if input is a command and 2. if such command belongs to this tab
	// 	if ((testParse.isCommand() == false) or
	// 		std::find(this->hints.begin(), this->hints.end(), toString(testParse.getCommandType())) == this->hints.end())
	// 		throw AppException({ErrorCode::BAD_MESSAGE});
	// }
	// catch (AppException const& e)
	// {
	// 	if (e.getError().code == ErrorCode::BAD_MESSAGE)
	// 	{
	// 		// update the window with 'wrong input'
	// 		return;
	// 	}
	// 	throw e;		// Qt callbacks do not support exceptions, we should never get here
	// }
	UI::handleCommand(input);

	if (this->pollFds[GUI::CLIENT].events & POLLOUT)
		this->writeClientNotifier->setEnabled(true);
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
