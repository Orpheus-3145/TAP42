#include "ErrorWindow.hpp"
#include "OutputTab.hpp"
#include "ButtonTab.hpp"
#include "CLI.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <cassert>
#include <format>


ErrorWindow::ErrorWindow(UI* engine) : TapWindow(engine)
{
	this->tabs[ErrorWindow::FRAME] = std::make_unique<BasicTab>(-1, RED_COLOR, this);
	this->tabs[ErrorWindow::INFO] = std::make_unique<OutputTab>("ERROR", 0, RED_COLOR, this);

	this->tabsToSkip.insert(ErrorWindow::FRAME);
	this->tabsToSkip.insert(ErrorWindow::INFO);
}

void ErrorWindow::draw(int32_t height, int32_t width)
{
	assert(this->tabs.find(ErrorWindow::ACTION1) != this->tabs.end() and "missing at least one action");

	this->height = (height % 2) == 0 ? height : height - 1;
	this->width = width;

	this->tabs.at(ErrorWindow::FRAME)->draw(
		this->height, 
		this->width,
		0,
		0
	);
	int32_t heightTabs = this->height / 2;
	int32_t widthTabs = this->width / 2;

	this->tabs.at(ErrorWindow::INFO)->draw(
		heightTabs,
		widthTabs,
		(this->height - heightTabs) / 2,
		(this->width - widthTabs) / 2
	);

	int32_t starty, startxAction1, startxAction2;
	ButtonTab* btnTab = dynamic_cast<ButtonTab*>(this->tabs.at(ErrorWindow::ACTION1).get());
	assert(btnTab != nullptr and "current tab doesn't support getWidth()");

	starty = (this->height - heightTabs) / 2 + heightTabs - 3 - 1;
	if (this->tabs.find(ErrorWindow::ACTION2) != this->tabs.end())
		startxAction1 = (this->width - widthTabs) / 2 + 2;
	else
		startxAction1 = (this->width - btnTab->getWidth()) / 2;

	this->tabs.at(ErrorWindow::ACTION1)->draw(
		1,
		1,
		starty,
		startxAction1
	);
	if (this->tabs.find(ErrorWindow::ACTION2) != this->tabs.end())
	{
		btnTab = dynamic_cast<ButtonTab*>(this->tabs.at(ErrorWindow::ACTION2).get());
		assert(btnTab != nullptr and "current tab doesn't support getWidth()");

		startxAction2 = startxAction1 + widthTabs - btnTab->getWidth() - 4;
		this->tabs.at(ErrorWindow::ACTION2)->draw(
			1,
			1,
			starty,
			startxAction2
		);
	}

	this->updateContentWindow();

	LOG_DEBUG(LogContext::INTERFACE, std::format("Showing error window, size h: {}, w: {}", this->height, this->width));
}

void ErrorWindow::clear(void) noexcept
{
	TapWindow::clear();

	this->tabs.erase(ErrorWindow::ACTION1);
	this->tabs.erase(ErrorWindow::ACTION2);

	OutputTab* descTab = dynamic_cast<OutputTab*>(this->tabs.at(ErrorWindow::INFO).get());
	assert(descTab != nullptr and "current tab doesn't support removing content");
	descTab->clearContent();

	this->activeTabIndex = 0;
}

void ErrorWindow::adaptWinToError(ErrorData& error) noexcept
{
	this->clear();

	OutputTab* descTab = dynamic_cast<OutputTab*>(this->tabs.at(ErrorWindow::INFO).get());
	assert(descTab != nullptr and "current tab doesn't support appending content");

	descTab->clearContent();
	descTab->appendContent(" ", TextAlign::LEFT, A_NORMAL, false);
	descTab->appendContent(mapError(error), TextAlign::MID, A_NORMAL, false);

	switch (error.code)
	{
		case ErrorCode::UI_USERNAME_NOT_EXISTS:
			this->setAction1("RETRY", [this] { this->engine->switchWindow(GamePhase::LOGIN); });
			this->setAction2("CREATE NEW", [this] { this->engine->switchWindow(GamePhase::PLAYER_CREATE);	});
			break;

		case ErrorCode::UI_USERNAME_IN_USE:
			this->setAction1("RETRY", [this] { this->engine->switchWindow(GamePhase::PLAYER_CREATE); });
			break;

		case ErrorCode::SERVER_ERROR:
			this->setAction1("CLOSE", [this] { this->engine->stop(); });
			this->setAction2("BACK", [this] { this->engine->switchWindow(); });
			break;
		
		case ErrorCode::SERVER_DISCONNECTED:
			this->setAction1("CONNECT", [this] { this->engine->reconnect(); });
			this->setAction2("CLOSE", [this] { this->engine->stop(); });
			break;

		default:
			this->setAction1("CLOSE", [this] { this->engine->stop(); });
			break;
	}

}

void ErrorWindow::setAction1(std::string const& actionName, std::function<void()> action)
{
	this->tabs[ErrorWindow::ACTION1] = std::make_unique<ButtonTab>(actionName, action, -1, this);
	this->switchActiveTab(ErrorWindow::ACTION1);
}

void ErrorWindow::setAction2(std::string const& actionName, std::function<void()> action)
{
	this->tabs[ErrorWindow::ACTION2] = std::make_unique<ButtonTab>(actionName, action, -1, this);
}
