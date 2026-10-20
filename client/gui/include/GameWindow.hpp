#pragma once

#include <memory>

#include "QtWindow.hpp"
#include "Panel.hpp"
#include "Message.hpp"


class GameWindow : public QtWindow
{
	Q_OBJECT

	public:
		using QtWindow::QtWindow;

		GameWindow(UI* engine);

		~GameWindow(void) noexcept {}

		void appendResponse(Message const& response, CommandType cmdType);
		void appendEvent(Message const& event);
		void appendError(Message const& error, CommandType cmdType);
		void updatePlayerPanel(PlayerInfo const& data);

	protected:
		void showEvent(QShowEvent* event) override;

	private:
		static constexpr int32_t WIDTH = 1100;
		static constexpr int32_t HEIGHT = 800;

		std::unique_ptr<Panel> playerPanel;
		std::unique_ptr<Panel> worldPanel;
		std::unique_ptr<Panel> cmdPanel;
		std::unique_ptr<Panel> chatPanel;
};
