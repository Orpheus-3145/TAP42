#include <format>
#include "Panel.hpp"
#include "GUI.hpp"
#include "Config.hpp"
#include "Message.hpp"
#include "Exceptions.hpp"

#include <string>
#include <QCompleter>
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>


Panel::Panel(
	QString const& title,
	QString const& color,
	bool withInput,
	bool showTimestamps,
	bool checkInput,
	QWidget* parent,
	std::optional<std::vector<std::string>> hints
) :
	QFrame(parent),
	color{color},
	showTimestamps{showTimestamps},
	checkInput{checkInput}
{
	this->setObjectName("panel");
	this->setStyleSheet(QString(
		"QFrame#panel      { border: 1px solid %1; background: #141420; }"
		"QLabel#title      { border: 1px solid %1; color: %1; font-weight: bold; padding: 6px 8px; }"
		"QFrame#rule       { border: none; background: %1; }"
		"QPlainTextEdit    { border: none; background: transparent; color: #e0e0e0; font-family: monospace; }"
		"QFrame#promptBox  { border: 1px solid %1; background: transparent; }"
		"QLabel#prefix     { border: none; color: %1; font-family: monospace; font-weight: bold; }"
		"QLineEdit         { border: none; background: transparent; color: #e0e0e0; font-family: monospace; }"
	).arg(this->color));

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(20, 20, 20, 20);
	layout->setSpacing(10);

	if (!title.isEmpty())
	{
		QHBoxLayout* header = new QHBoxLayout();
		header->setSpacing(15);

		QLabel* titleLabel = new QLabel(title, this);
		titleLabel->setObjectName("title");

		QFrame* rule = new QFrame(this);
		rule->setObjectName("rule");
		rule->setFixedHeight(1);

		header->addWidget(titleLabel, 0);
		header->addWidget(rule, 1, Qt::AlignBottom);
		layout->addLayout(header);
	}

	this->log = new QPlainTextEdit(this);
	this->log->setReadOnly(true);
	layout->addWidget(this->log, 1);

	if (withInput)
	{
		QFrame* box = new QFrame(this);
		box->setObjectName("promptBox");
		box->setFixedHeight(40);

		QHBoxLayout* boxLayout = new QHBoxLayout(box);
		boxLayout->setContentsMargins(10, 0, 10, 0);

		QLabel* prefix = new QLabel(Config::PROMPT, box);
		prefix->setObjectName("prefix");
		boxLayout->addWidget(prefix);

		this->prompt = new QLineEdit(box);
		if (hints.has_value())
		{
			for (std::string const& cmd : hints.value())
				this->hints.append(QString(cmd.data()));
			QCompleter* completer = new QCompleter(this->hints, this->prompt);
			completer->setCompletionMode(QCompleter::InlineCompletion);
			this->prompt->setCompleter(completer);
		}

		boxLayout->addWidget(this->prompt, 1);
		layout->addWidget(box, 0);

		QObject::connect(
			this->prompt,
			&QLineEdit::returnPressed,
			this,
			&Panel::submitCommand
		);
	}
}

void Panel::appendInput(std::string const& text, std::optional<std::string> color, bool addTimestamps)
{
	auto now = std::chrono::system_clock::now();
	auto timestamp = std::chrono::time_point_cast<std::chrono::seconds>(now);

	std::string textColor = color.has_value() ? color.value() : this->color.toStdString();
	std::stringstream ss(text);
	std::string line;
	std::getline(ss, line, '\n');
	if (addTimestamps)
		this->state.push_back(std::format("[{:%H:%M:%S}]<span style='white-space: pre; color: {}'>  {}</span>", timestamp, textColor, line));
	else if (this->showTimestamps)
		this->state.push_back(std::format("<span style='white-space: pre; color: {}'>            {}</span>", textColor, line));
	else
		this->state.push_back(std::format("<span style='white-space: pre; color: {}'>  {}</span>", textColor, line));
	this->log->appendHtml(QString::fromStdString(this->state.back()));

	while (std::getline(ss, line, '\n'))
	{
		if (this->showTimestamps)
			this->state.push_back(std::format("<span style='white-space: pre; color: {}'>            {}</span>", textColor, line));
		else
			this->state.push_back(std::format("<span style='white-space: pre; color: {}'>  {}</span>", textColor, line));
		this->log->appendHtml(QString::fromStdString(this->state.back()));
	}
}

void Panel::refresh(void)
{
	this->clear();
	for (std::string const& line : this->state)
		this->log->appendHtml(QString::fromStdString(line.data()));
}

void Panel::submitCommand(void)
{
	std::string const input = this->prompt->text().toStdString();
	if (input.empty())
		return;
	this->prompt->clear();

	this->appendInput(input, QT_WHITE_COLOR, true);
	if (this->checkInput)
	{
		try
		{
			Message testParse(input);
			// check 1. if input is a command and 2. if such command belongs to this tab
			if ((testParse.isCommand() == false) or
				std::find(this->hints.begin(), this->hints.end(), QString(toString(testParse.getCommandType()).data())) == this->hints.end())
				throw AppException({ErrorCode::BAD_MESSAGE});

			emit submitted(QString(input.data()));
		}
		catch (AppException const& e)
		{
			this->appendInput("Invalid command", QT_RED_COLOR);
		}
	}
}