#pragma once

#include <memory>
#include <map>
#include <set>
#include <cstdint>

#include <QMainWindow>
#include <QLineEdit>

#include "UI.hpp"


// Stile "terminale" condiviso da tutti i pannelli: sfondo scuro, testo bianco,
// font monospace, bordo sottile bianco — per assomigliare all'immagine di riferimento.
static const QString kPanelStyle = R"(
    background-color: #1a1a1a;
    color: #e6e6e6;
    border: 1px solid #ffffff;
    font-family: "Courier New", monospace;
    font-size: 13px;
)";

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

		// void		switchActiveTab(int32_t index = -1);
		// BasicTab*	getActiveTab(void) { return this->tabs.at(this->activeTabIndex).get(); }

	// private slots:
	// 	void onPromptSubmitted(void);

	protected:
		virtual void draw(int32_t height, int32_t width);

		UI* const	engine;

		// std::map<size_t,std::unique_ptr<BasicTab>>	tabs{};
		// std::set<size_t>							tabsToSkip{};

		// int32_t activeTabIndex{0};
};
