#pragma once

#include <memory>
#include <map>
#include <set>
#include <cstdint>

#include <QMainWindow>

#include "UI.hpp"


class UI;

class Window : public QMainWindow
{
    Q_OBJECT

	public:
		Window(QWidget *parent, UI* engine);

		Window(Window const& other) = delete;
		Window& operator=(Window const& other) = delete;
		Window(Window&& other) = delete;
		Window& operator=(Window&& other) = delete;

		virtual ~Window(void) noexcept;

		// void		switchActiveTab(int32_t index = -1);
		// BasicTab*	getActiveTab(void) { return this->tabs.at(this->activeTabIndex).get(); }

	protected:
		virtual void draw(int32_t height, int32_t width) = 0;

		UI* const engine;

		// std::map<size_t,std::unique_ptr<BasicTab>>	tabs{};
		// std::set<size_t>							tabsToSkip{};

		// int32_t activeTabIndex{0};
};
