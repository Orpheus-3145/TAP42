#pragma once

#include <memory>
#include <map>
#include <set>
#include <cstdint>

#include <QMainWindow>
#include <QLineEdit>

#include "UI.hpp"


class UI;

class Window : public QMainWindow
{
    Q_OBJECT

	public:
		Window(int32_t height, int32_t width, UI* engine, QWidget *parent = nullptr);

		Window(Window const& other) = delete;
		Window& operator=(Window const& other) = delete;
		Window(Window&& other) = delete;
		Window& operator=(Window&& other) = delete;

		virtual ~Window(void) noexcept;

		virtual void draw(void) = 0;

	signals:
		void commandEntered(std::string input);

	protected:
		UI* const	engine;
};
