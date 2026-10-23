#pragma once

#include <memory>
#include <map>
#include <set>
#include <cstdint>

#include <QMainWindow>
#include <QLineEdit>

#include "UI.hpp"


class UI;

class QtWindow : public QMainWindow
{
    Q_OBJECT

	public:
		QtWindow(UI* engine);

		QtWindow(QtWindow const& other) = delete;
		QtWindow& operator=(QtWindow const& other) = delete;
		QtWindow(QtWindow&& other) = delete;
		QtWindow& operator=(QtWindow&& other) = delete;

		virtual ~QtWindow(void) noexcept;

	signals:
		void commandEntered(std::string input);

	protected:
		void showEvent(QShowEvent* event) override;		// Qt override, has nothing to do with events

		UI* const	engine;
};
