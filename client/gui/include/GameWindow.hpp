#pragma once

#include <vector>

#include "QtWindow.hpp"
#include "Panel.hpp"
#include "Message.hpp"


class GameWindow : public QtWindow
{
	Q_OBJECT

	public:
		using QtWindow::QtWindow;

		~GameWindow(void) noexcept {}

		void draw(void) override;

		void appendResponse(Message const& response, CommandType cmdType);
		void appendEvent(Message const& event);
		void appendError(Message const& error, CommandType cmdType);

	private:
		static constexpr int32_t WIDTH = 1100;
		static constexpr int32_t HEIGHT = 800;

		Panel* playerPanel{nullptr};
		Panel* worldPanel{nullptr};
		Panel* cmdPanel{nullptr};
		Panel* chatPanel{nullptr};
};
