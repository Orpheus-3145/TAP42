#pragma once

#include <QApplication>
#include <QObject>
#include <QSocketNotifier>

#include <cstdint>
#include <string>

#include "UI.hpp"
#include "LoginWindow.hpp"
#include "PlayerCreateWindow.hpp"
#include "GameWindow.hpp"
#include "ErrorWindow.hpp"


static constexpr char const* BLUE_COLOR = "#1f5fa8";
static constexpr char const* RED_COLOR = "#ff0000";
static constexpr char const* GREEN_COLOR = "#1fa870";
static constexpr char const* YELLOW_COLOR = "#b87333";
static constexpr char const* CYAN_COLOR = "#1fa3b8";

class GUI : public QObject, public UI
{
	Q_OBJECT

	public:
		GUI(int32_t height, int32_t width);

		GUI(GUI const& other) noexcept = delete;
		GUI& operator=(GUI const& other) noexcept = delete;
		GUI(GUI&& other) noexcept = delete;
		GUI& operator=(GUI&& other) noexcept = delete;

		~GUI(void) noexcept override;

		void start(void) override;
		void stop(void) noexcept override;
		void switchWindow(std::optional<GamePhase> newPhase = std::nullopt) override;

	private slots:
		void writeToServer(void) override;
		void readFromServer(void) override;
		void handleCommand(std::string const& input) override;

	private:
		void handleException(ErrorData const& error) noexcept override;

		void showResponse(Message const& response) noexcept override;
		void showEvent(Message const& event) noexcept override;
		void showError(Message const& error) noexcept override;

		std::unique_ptr<QApplication>		app;
		std::unique_ptr<QSocketNotifier>	readClientNotifier, writeClientNotifier;

		std::unique_ptr<LoginWindow>		loginWin;
		std::unique_ptr<PlayerCreateWindow>	newPlayerWin;
		std::unique_ptr<GameWindow>			gameWin;
		std::unique_ptr<ErrorWindow>		errorWin;

		Window* currentWindow{nullptr};
};
