#include "GameWindow.hpp"

GameWindow::GameWindow(QWidget *parent, int32_t height, int32_t width) :
	QMainWindow(parent)
{
	setWindowTitle("TAP");
	resize(width, height);

	QWidget *central = new QWidget(this);
	setCentralWidget(central);

	QHBoxLayout *rootLayout = new QHBoxLayout(central);
	rootLayout->setContentsMargins(10, 10, 10, 10);
	rootLayout->setSpacing(10);

	// ---------- Pannello sinistro: INPUT ----------
	QWidget *leftPanel = new QWidget(central);
	QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	leftLayout->setSpacing(0);

	inputLog = new QPlainTextEdit(leftPanel);
	inputLog->setReadOnly(true);
	inputLog->setStyleSheet(kPanelStyle);
	inputLog->setPlainText("This is where the input is shown");

	promptField = new QLineEdit(leftPanel);
	promptField->setStyleSheet(kPanelStyle);
	promptField->setText("-> ");
	promptField->setFrame(false);

	leftLayout->addWidget(inputLog, /*stretch=*/1);
	leftLayout->addWidget(promptField, /*stretch=*/0);

	// ---------- Pannello destro: OUTPUT (risposte + eventi) ----------
	QSplitter *rightSplitter = new QSplitter(Qt::Vertical, central);

	responsesLog = new QPlainTextEdit(rightSplitter);
	responsesLog->setReadOnly(true);
	responsesLog->setStyleSheet(kPanelStyle);
	responsesLog->setPlainText("This is where responses are shown");

	eventsLog = new QPlainTextEdit(rightSplitter);
	eventsLog->setReadOnly(true);
	eventsLog->setStyleSheet(kPanelStyle);
	eventsLog->setPlainText("This is where events are shown");

	rightSplitter->addWidget(responsesLog);
	rightSplitter->addWidget(eventsLog);
	rightSplitter->setSizes({600, 400}); // proporzione iniziale, l'utente può trascinare

	// ---------- Composizione finale ----------
	rootLayout->addWidget(leftPanel, /*stretch=*/1);
	rootLayout->addWidget(rightSplitter, /*stretch=*/1);
}
