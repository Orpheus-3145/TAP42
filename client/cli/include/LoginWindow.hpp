#pragma once

#include "CurseWindow.hpp"
#include "CurseTab.hpp"


class LoginWindow : public CurseWindow
{
	public:
		LoginWindow(int32_t height, int32_t width, int32_t commandFd);

		virtual ~LoginWindow(void) noexcept { this->clear(); }

		void draw(void) override;
		void clear(void) noexcept override;
		void readInput(void) override;
		void resize(int32_t height, int32_t width) override;
		void switchInputTab(void) noexcept override {}
		void scrollTab(bool goingUp) noexcept override { (void) goingUp; }

		void handleResponse(std::string const& response) noexcept override { (void) response; }
		void handleChatMsg(std::string const& response) noexcept override { (void) response; }
		void handleEvent(std::string const& event) noexcept override { (void) event; }

	protected:
		static constexpr int32_t INFO_COLOR = 1;

		int32_t commandFd;
		int32_t messageFd;

		std::unique_ptr<BasicTab>		tmp, frame;
		std::unique_ptr<OutputTab>		outputNameTab;
		std::unique_ptr<SingleInputTab>	inputNameTab;
};
