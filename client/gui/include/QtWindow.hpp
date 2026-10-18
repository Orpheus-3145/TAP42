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
		QtWindow(int32_t height, int32_t width, UI* engine, QWidget *parent = nullptr);

		QtWindow(QtWindow const& other) = delete;
		QtWindow& operator=(QtWindow const& other) = delete;
		QtWindow(QtWindow&& other) = delete;
		QtWindow& operator=(QtWindow&& other) = delete;

		virtual ~QtWindow(void) noexcept;

		virtual void draw(void) = 0;

	signals:
		void commandEntered(std::string input);

	protected:
		UI* const	engine;
};
