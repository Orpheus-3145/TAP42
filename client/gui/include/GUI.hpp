#pragma once

#include <QApplication>
#include <QObject>
#include <QSocketNotifier>

#include <cstdint>
#include <string>

#include "UI.hpp"
#include "GameWindow.hpp"


class GUI : public QObject, public UI
{
	Q_OBJECT

	public:
		GUI(int32_t clientSocket, int32_t height, int32_t width);

		GUI(GUI const& other) noexcept = delete;
		GUI& operator=(GUI const& other) noexcept = delete;
		GUI(GUI&& other) noexcept = delete;
		GUI& operator=(GUI&& other) noexcept = delete;

		~GUI(void) noexcept override;

		void startUI(void) override { this->app->exec(); }
		void stopUI(void) noexcept override { this->app->quit(); }
		void resize(int32_t height, int32_t width) override { this->gameWin->resize(width, height); }
		
	private:
		void handleCommand(std::string const& command) override;
		void handleResponse(std::string const& response) noexcept override;
		void handleEvent(std::string const& event) noexcept override;

		std::unique_ptr<QApplication>	app;
		std::unique_ptr<GameWindow>		gameWin;

		std::unique_ptr<QSocketNotifier> readFromServerNotifier;
};
