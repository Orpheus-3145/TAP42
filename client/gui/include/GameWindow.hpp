#pragma once

#include <vector>

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QSplitter>
#include <QFont>
#include <QSocketNotifier>

#include "Window.hpp"
#include "Panel.hpp"
#include "Message.hpp"


class GameWindow : public Window
{
	Q_OBJECT

	public:
		using Window::Window;

		~GameWindow(void) noexcept {}

		void draw(void) override;

		void appendResponse(Message const& response, CommandType cmdType);
		void appendEvent(Message const& event);
		void appendError(Message const& error, CommandType cmdType);

	private:
		Panel* playerPanel{nullptr};
		Panel* worldPanel{nullptr};
		Panel* cmdPanel{nullptr};
		Panel* chatPanel{nullptr};
};
