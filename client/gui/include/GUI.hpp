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


static constexpr char const* QT_BLUE_COLOR = "#1f5fa8";
static constexpr char const* QT_RED_COLOR = "#ff0000";
static constexpr char const* QT_GREEN_COLOR = "#1fa870";
static constexpr char const* QT_YELLOW_COLOR = "#b87333";
static constexpr char const* QT_CYAN_COLOR = "#1fa3b8";
static constexpr char const* QT_WHITE_COLOR = "#ffffff";

class GUI : public QObject, public UI
{
	Q_OBJECT

	public:
		GUI(void);

		GUI(GUI const& other) noexcept = delete;
		GUI& operator=(GUI const& other) noexcept = delete;
		GUI(GUI&& other) noexcept = delete;
		GUI& operator=(GUI&& other) noexcept = delete;

		virtual ~GUI(void) noexcept override;

		void start(void) override;
		void stop(void) noexcept override;
		void switchWindow(std::optional<GamePhase> newPhase = std::nullopt) override;

	private slots:
		void writeToServer(void) override;
		void readFromServer(void) override;

	private:
		void handleException(ErrorData const& error) noexcept override;

		void showResponse(Message const& response) noexcept override;
		void showEvent(Message const& event) noexcept override;
		void showError(Message const& error) noexcept override;

		void startSpeaking(void) override { this->writeClientNotifier->setEnabled(true); }
		void stopSpeaking(void) override { this->writeClientNotifier->setEnabled(false); }

		std::unique_ptr<QApplication>		app;
		std::unique_ptr<QSocketNotifier>	readClientNotifier, writeClientNotifier;

		std::unique_ptr<LoginWindow>		loginWin;
		std::unique_ptr<PlayerCreateWindow>	newPlayerWin;
		std::unique_ptr<GameWindow>			gameWin;
		std::unique_ptr<ErrorWindow>		errorWin;

		QtWindow* currentWindow{nullptr};
};
