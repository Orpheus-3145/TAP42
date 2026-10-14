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

	this->readClientNotifier = new QSocketNotifier(this->pollFds[GUI::CLIENT].fd, QSocketNotifier::Read, this);
	QObject::connect(
		this->readClientNotifier,
		&QSocketNotifier::activated,
		this,
		&CLI::readFromServer
	);

	this->writeClientNotifier = new QSocketNotifier(this->pollFds[GUI::CLIENT].fd, QSocketNotifier::Write, this);
	this->writeClientNotifier->setEnabled(false);
	QObject::connect(
		this->writeClientNotifier,
		&QSocketNotifier::activated,
		this,
		&CLI::writeToServer
	);

	this->commandNotifier = new QSocketNotifier(this->pollFds[GUI::CMD].fd, QSocketNotifier::Read, this);
	QObject::connect(
		this->commandNotifier,
		&QSocketNotifier::activated,
		this,
		&CLI::handleCommand
	);

	this->loginWin = std::make_unique<LoginWindow>(nullptr, this, height, width);
	this->newPlayerWin = std::make_unique<PlayerCreateWindow>(nullptr, this, height, width);
	this->gameWin = std::make_unique<GameWindow>(nullptr, this, height, width);
	this->errorWin = std::make_unique<ErrorWindow>(nullptr, this, height, width);

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

void GUI::writeToServer(void)
{
	try
	{
		UI::writeToServer();
		if (this->pollFds[GUI::CLIENT].revents == POLLHUP)
			throw AppException({ErrorCode::CLIENT_DISCONNECTED});
		else if (this->pollFds[GUI::CLIENT].events == POLLIN)
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
		if (this->pollFds[GUI::CLIENT].revents == POLLHUP)
			throw AppException({ErrorCode::CLIENT_DISCONNECTED});
		else if (this->pollFds[GUI::CLIENT].events == POLLOUT)
			this->writeClientNotifier->setEnabled(true);
	}
	catch (AppException const& e)
	{
		this->handleException(e.getError());
	}
}

Sì, in Qt la pipe non serve più. Serviva solo perché il `poll()` bloccava il thread e l'unico modo di svegliarlo era scrivere su un fd. Con l'event loop di Qt basta un **segnale**: il widget emette un segnale all'invio, e lo slot che lo riceve inoltra il comando al server.

## Lato widget

`QLineEdit` ha già il segnale `returnPressed()`, quindi spesso non serve nemmeno una classe custom:

```cpp
class InputWidget : public QWidget {
    Q_OBJECT
public:
    explicit InputWidget(QWidget *parent = nullptr) : QWidget(parent) {
        m_edit = new QLineEdit(this);
        // layout...

        connect(m_edit, &QLineEdit::returnPressed, this, [this] {
            const QString cmd = m_edit->text();
            m_edit->clear();
            if (!cmd.isEmpty())
                emit commandEntered(cmd);
        });
    }

signals:
    void commandEntered(const QString &cmd);

private:
    QLineEdit *m_edit;
};
```

## Lato GUI / CLI

```cpp
connect(inputWidget, &InputWidget::commandEntered,
        this, &GUI::onCommand);

void GUI::onCommand(const QString &cmd) {
    // 1. eventuale elaborazione/echo locale nella GUI
    appendToOutput("> " + cmd);

    // 2. inoltro al server
    m_cli->queueForServer(cmd.toUtf8() + '\n');
}
```

Oppure, se la GUI non deve toccare il comando, colleghi direttamente il segnale a `CLI`:

```cpp
connect(inputWidget, &InputWidget::commandEntered,
        m_cli, &CLI::sendCommand);
```

## Cosa sparisce

- La pipe `CMD` e il suo `QSocketNotifier` (`m_cmdNotifier`, `m_cmdFd`).
- `handleCommand()` come lettura da fd: diventa lo slot `onCommand(const QString &)`, che riceve già il comando e non deve fare `read()` né parsing del buffer.
- Il problema dei comandi spezzati o concatenati: con una pipe potevi leggere mezzo comando o due insieme, con i segnali ricevi sempre una stringa completa.

## Nota sui thread

Se widget e `CLI` stanno nello stesso thread (il caso normale, la GUI), la connessione è diretta e immediata. Se `CLI` è in un altro `QThread`, Qt usa automaticamente una connessione queued, quindi è thread-safe senza fare altro. In quel caso `queueForServer` verrebbe eseguito nel thread di `CLI`, dove vive anche il `QSocketNotifier`, ed è quello che vuoi: **i notifier e le chiamate che li toccano (`setEnabled`) devono stare nel thread a cui appartengono**.

Se la pipe serve anche a un processo esterno, allora ha senso tenerla con il suo `QSocketNotifier`. Se è solo comunicazione interna widget → GUI, il segnale è la soluzione giusta.

void GUI::handleCommand(void)
{
	// UI::handleCommand();
	// if (this->pollFds[GUI::CLIENT].events == POLLOUT)
	// 	this->writeClientNotifier->setEnabled(true);

	if (input.isEmpty())
		return;

	const std::string text = input.toStdString();
	std::string command;

	if (this->phase == GamePhase::LOGIN)
	{
		this->username = text;
		command = std::format("{} {}", toString(CommandType::CONNECT), this->username);
	}
	else if (this->phase == GamePhase::PLAYER_CREATE)
	{
		this->username = text;
		command = std::format("{} {}", toString(CommandType::CREATE_PLAYER), this->username);
	}
	else
		command = text;

	this->commandQueue.push(Message(command));
	LOG_DEBUG(LogContext::INTERFACE, std::format("Command '{}' added to queue", command));

	this->writeNotifier->setEnabled(true);
}

void GUI::handleException(ErrorData& error) noexcept
{
	UI::handleException(error);
	// NB ...
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
