#include "Panel.hpp"


Panel::Panel(
	QString const& title,
	QString const& color,
	bool withInput,
	QString const& promptPrefix,
	QWidget* parent
) :
	QFrame(parent)
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
	).arg(color));

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

		if (!promptPrefix.isEmpty())
		{
			QLabel* prefix = new QLabel(promptPrefix, box);
			prefix->setObjectName("prefix");
			boxLayout->addWidget(prefix);
		}

		this->prompt = new QLineEdit(box);
		boxLayout->addWidget(this->prompt, 1);
		layout->addWidget(box, 0);

		QObject::connect(this->prompt, &QLineEdit::returnPressed, this, [this] {
			const QString text = this->prompt->text();
			if (text.isEmpty())
				return;
			this->prompt->clear();
			emit submitted(text);
		});
	}
}