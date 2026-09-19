#pragma once

#include "CurseWindow.hpp"
#include "CurseTab.hpp"


class PlayerCreateWindow : public CurseWindow
{
	public:
		PlayerCreateWindow(int32_t commandFd);

		virtual ~PlayerCreateWindow(void) noexcept { this->clear(); }

		void draw(int32_t height, int32_t width) override;
		void clear(void) noexcept override;
		void readInput(void) override;
		void resize(int32_t height, int32_t width) override;
		void switchInputTab(void) noexcept override {}
		void scrollTab(bool goingUp) noexcept override { (void) goingUp; }

		void showResponse(std::string const& response) noexcept override { (void) response; }
		void showChatMsg(std::string const& response) noexcept override { (void) response; }
		void showEvent(std::string const& event) noexcept override { (void) event; }

	protected:
		static constexpr int32_t INFO_COLOR = 6;

		int32_t commandFd;
		int32_t messageFd;

		std::unique_ptr<BasicTab>		tmp, frame;
		std::unique_ptr<OutputTab>		outputNameTab;
		std::unique_ptr<SingleInputTab>	inputNameTab;
};
