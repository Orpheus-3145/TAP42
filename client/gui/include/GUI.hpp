#pragma once

#include <QApplication>
#include <cstdint>

#include "UI.hpp"
#include "GameWindow.hpp"


class GUI : public UI
{
	public:
		GUI(int32_t clientSocket, int32_t height, int32_t width);

		GUI(GUI const& other) noexcept = delete;
		GUI& operator=(GUI const& other) noexcept = delete;
		GUI(GUI&& other) noexcept = delete;
		GUI& operator=(GUI&& other) noexcept = delete;

		~GUI(void) noexcept override {};

		void loop(void) override { this->app.exec();}
		
	private:
		void handleCommand(void) override;
		void handleResponse(std::string const& response) noexcept override;
		void handleEvent(std::string const& event) noexcept override;

		void resize(int32_t height, int32_t width) override { this->mainWindow.resize(width, height); }
		void refresh(void) noexcept override;

		QApplication	app;
		GameWindow		mainWindow;
};
